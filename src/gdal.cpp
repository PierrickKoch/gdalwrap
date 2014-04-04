/*
 * gdal.cpp
 *
 * C++11 GDAL wrapper
 *
 * author:  Pierrick Koch <pierrick.koch@laas.fr>
 * created: 2013-09-22
 * license: BSD
 */

#include <string>
#include <iostream>         // cout,cerr,endl
#include <stdexcept>        // for runtime_error
#include <gdal_priv.h>      // for GDALDataset
#include <ogr_spatialref.h> // for OGRSpatialReference
#include <cpl_string.h>     // for CSLSetNameValue

#include "gdalwrap/gdal.hpp"

namespace gdalwrap {

/** Set the WGS84 projection
 */
inline void set_wgs84(GDALDataset *dataset, int utm_zone, int utm_north) {
    OGRSpatialReference spatial_reference;
    char *projection = NULL;

    spatial_reference.SetUTM( utm_zone, utm_north );
    spatial_reference.SetWellKnownGeogCS( "WGS84" );
    spatial_reference.exportToWkt( &projection );
    dataset->SetProjection( projection );
    CPLFree( projection );
}

void gdal::_init() {
    // Register all known configured GDAL drivers.
    GDALAllRegister();
    set_transform(0, 0);
    set_custom_origin(0, 0, 0);
    set_utm(0);
}

/** Save as GeoTiff
 *
 * @param filepath path to .tif file.
 */
void gdal::save(const std::string& filepath) const {
    // get the GDAL GeoTIFF driver
    GDALDriver *driver = GetGDALDriverManager()->GetDriverByName("GTiff");
    if ( driver == NULL )
        throw std::runtime_error("[gdal] could not get the driver");

    char ** options = NULL;
    // fastest deflate (zlib/png)
    options = CSLSetNameValue( options, "COMPRESS",     "DEFLATE" );
    options = CSLSetNameValue( options, "PREDICTOR",    "1" );
    options = CSLSetNameValue( options, "ZLEVEL",       "1" );

    // create the GDAL GeoTiff dataset (n layers of float32)
    GDALDataset *dataset = driver->Create( filepath.c_str(), width, height,
        bands.size(), GDT_Float32, options );
    if ( dataset == NULL )
        throw std::runtime_error("[gdal] could not create (multi-layers float32)");

    set_wgs84(dataset, utm_zone, utm_north);
    // see GDALDataset::GetGeoTransform()
    dataset->SetGeoTransform( (double *) transform.data() );
    // Set dataset metadata
    for (const auto& pair : metadata)
        dataset->SetMetadataItem( pair.first.c_str(), pair.second.c_str() );

    GDALRasterBand *band;
    for (size_t band_id = 0; band_id < bands.size(); band_id++) {
        band = dataset->GetRasterBand(band_id+1);
        band->RasterIO( GF_Write, 0, 0, width, height,
            (void *) bands[band_id].data(), width, height, GDT_Float32, 0, 0 );
        band->SetMetadataItem("NAME", names[band_id].c_str());
    }

    // close properly the dataset
    GDALClose( (GDALDatasetH) dataset );
    CSLDestroy( options );
}

/** Load a GeoTiff
 *
 * @param filepath path to .tif file.
 */
void gdal::load(const std::string& filepath) {
    // Open a raster file as a GDALDataset.
    GDALDataset *dataset = (GDALDataset *) GDALOpen( filepath.c_str(), GA_ReadOnly );
    if ( dataset == NULL )
        throw std::runtime_error("[gdal] could not open the given file");

    std::string _type = GDALGetDriverShortName( dataset->GetDriver() );
    if ( _type.compare( "GTiff" ) != 0 )
        std::cerr<<"[warn]["<< __func__ <<"] expected GTiff and got: "<<_type<<std::endl;

    set_size( dataset->GetRasterCount(), dataset->GetRasterXSize(),
        dataset->GetRasterYSize() );

    // get utm zone
    OGRSpatialReference spatial_reference( dataset->GetProjectionRef() );
    int _north;
    utm_zone = spatial_reference.GetUTMZone( &_north );
    utm_north = (_north != 0);

    // GetGeoTransform returns CE_Failure if the transform is not found
    // as well as when it's the default {0.0, 1.0, 0.0, 0.0, 0.0, 1.0}
    // and write {0.0, 1.0, 0.0, 0.0, 0.0, 1.0} in transform anyway
    // so error handling here is kind of useless...
    dataset->GetGeoTransform( transform.data() );
    // Parse dataset metadata
    // GetMetadata returns a string list owned by the object,
    // and may change at any time. It is formated as a "Name=value" list
    // with the last pointer value being NULL.
    char **_metadata = dataset->GetMetadata();
    if(_metadata != NULL) {
        for (int meta_id = 0; _metadata[meta_id] != NULL; meta_id++) {
            std::string item( _metadata[meta_id] );
            std::string::size_type n = item.find('=');
            // k,v = item.split('=')
            metadata[ item.substr(0, n) ] = item.substr(n + 1);
        }
    }

    GDALRasterBand *band;
    const char *name;
    for (size_t band_id = 0; band_id < bands.size(); band_id++) {
        band = dataset->GetRasterBand(band_id+1);
        if ( band->GetRasterDataType() != GDT_Float32 )
            std::cerr<<"[warn]["<< __func__ <<"] only support Float32 bands"<<std::endl;
        band->RasterIO( GF_Read, 0, 0, width, height,
            bands[band_id].data(), width, height, GDT_Float32, 0, 0 );
        name = band->GetMetadataItem("NAME");
        if (name != NULL)
            names[band_id] = name;
    }

    // close properly the dataset
    GDALClose( (GDALDatasetH) dataset );

    // WARN std::stod might throw std::invalid_argument
    custom_x_origin = std::stod( get_meta("CUSTOM_X_ORIGIN", "0") );
    custom_y_origin = std::stod( get_meta("CUSTOM_Y_ORIGIN", "0") );
    custom_z_origin = std::stod( get_meta("CUSTOM_Z_ORIGIN", "0") );
}

/** Export a band as Byte
 *
 * Distribute the height using `vfloat2vuchar` method.
 * Guess the driver shortname.
 *
 * @param filepath path to .{jpg,gif,png} file.
 * @param band number [0,n-1].
 */
void gdal::export8u(const std::string& filepath, int band) const {
    std::string ext = toupper( filepath.substr( filepath.rfind(".") + 1 ) );

    if (!ext.compare("JPG"))
        ext = "JPEG";

    export8u(filepath, band, ext);
}

/** Export a band as Byte
 *
 * Distribute the height using `vfloat2vuchar` method.
 * First create a temporary GeoTiff file with the Byte band,
 * and then copy it to the `filepath` with the correct driver.
 * Because `Create` is not supported by all driver, but `CreateCopy` is.
 *
 * @param filepath path to .{jpg,gif,png} file.
 * @param band number [0,n-1].
 * @param driver_shortname see http://gdal.org/formats_list.html
 */
void gdal::export8u(const std::string& filepath, int band,
                    const std::string& driver_shortname) const {
    // get the driver from its shortname
    GDALDriver *driver = GetGDALDriverManager()->GetDriverByName( driver_shortname.c_str() );
    if ( driver == NULL )
        throw std::runtime_error("[gdal] could not get the driver: " + driver_shortname);
    // get the GDAL GeoTIFF driver
    GDALDriver *drtiff = GetGDALDriverManager()->GetDriverByName("GTiff");
    if ( drtiff == NULL )
        throw std::runtime_error("[gdal] could not get the GTiff driver");

    // could use something like tempnam(dirname(filepath), NULL)
    // but it does not garantee the result to be local, if TMPDIR is set.
    // and std::rename(2) works only locally, not across disks.
    std::string tmptif = filepath + ".tif.export8u.tmp";
    std::string tmpres = filepath +     ".export8u.tmp";
    // create the GDAL GeoTiff dataset (1 layers of byte)
    GDALDataset *dataset = drtiff->Create( tmptif.c_str(), width, height,
        1, GDT_Byte, NULL );
    if ( dataset == NULL )
        throw std::runtime_error("[gdal] could not create dataset");

    set_wgs84(dataset, utm_zone, utm_north);
    // see GDALDataset::GetGeoTransform()
    dataset->SetGeoTransform( (double *) transform.data() );
    // set dataset metadata
    for (const auto& pair : metadata)
        dataset->SetMetadataItem( pair.first.c_str(), pair.second.c_str() );

    // convert the band from float to byte
    bytes_t band8u ( raster2bytes( bands[band] ) );

    GDALRasterBand *raster_band = dataset->GetRasterBand(1);
    raster_band->RasterIO( GF_Write, 0, 0, width, height,
        (void *) band8u.data(), width, height, GDT_Byte, 0, 0 );
    raster_band->SetMetadataItem("NAME", names[band].c_str());
    // save initial min/max
    auto minmax = std::minmax_element(bands[band].begin(), bands[band].end());
    raster_band->SetMetadataItem("INITIAL_MIN", std::to_string(*minmax.first).c_str());
    raster_band->SetMetadataItem("INITIAL_MAX", std::to_string(*minmax.second).c_str());

    char ** options = NULL;
    if (!driver_shortname.compare("JPEG"))
        options = CSLSetNameValue( options, "QUALITY", "95" );

    GDALDataset *copy = driver->CreateCopy( tmpres.c_str(), dataset, 0, options,
        NULL, NULL );

    if ( copy != NULL )
        GDALClose( (GDALDatasetH) copy );
    else
        std::cerr<<"[warn]["<< __func__ <<"] could not CreateCopy"<<std::endl;
    CSLDestroy( options );

    // close properly the dataset
    GDALClose( (GDALDatasetH) dataset );
    std::remove( tmptif.c_str() );
    std::string srcaux = tmpres   + ".aux.xml";
    std::string dstaux = filepath + ".aux.xml";
    std::rename( srcaux.c_str(), dstaux.c_str()   );
    std::rename( tmpres.c_str(), filepath.c_str() );
}

} // namespace gdalwrap
