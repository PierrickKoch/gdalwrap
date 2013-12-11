/*
 * gdal.cpp
 *
 * Graph Library for Autonomous and Dynamic Systems
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

void gdal::_init() {
    // Register all known configured GDAL drivers.
    GDALAllRegister();
    set_transform(0, 0);
    set_custom_origin(0, 0);
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

    // create the GDAL GeoTiff dataset (n layers of float32)
    GDALDataset *dataset = driver->Create( filepath.c_str(), width, height,
        bands.size(), GDT_Float32, NULL );
    if ( dataset == NULL )
        throw std::runtime_error("[gdal] could not create (multi-layers float32)");

    // set the projection
    OGRSpatialReference spatial_reference;
    char *projection = NULL;

    spatial_reference.SetUTM( utm_zone, utm_north );
    spatial_reference.SetWellKnownGeogCS( "WGS84" );
    spatial_reference.exportToWkt( &projection );
    dataset->SetProjection( projection );
    CPLFree( projection );

    // see GDALDataset::GetGeoTransform()
    dataset->SetGeoTransform( (double *) transform.data() );
    dataset->SetMetadataItem("CUSTOM_X_ORIGIN", std::to_string(custom_x_origin).c_str());
    dataset->SetMetadataItem("CUSTOM_Y_ORIGIN", std::to_string(custom_y_origin).c_str());

    GDALRasterBand *band;
    for (int band_id = 0; band_id < bands.size(); band_id++) {
        band = dataset->GetRasterBand(band_id+1);
        band->RasterIO( GF_Write, 0, 0, width, height,
            (void *) bands[band_id].data(), width, height, GDT_Float32, 0, 0 );
        band->SetMetadataItem("NAME", names[band_id].c_str());
    }

    // close properly the dataset
    GDALClose( (GDALDatasetH) dataset );
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
        std::cerr<<"[warn] expected GTiff and got: "<<_type<<std::endl;

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
    const char *cxo = dataset->GetMetadataItem("CUSTOM_X_ORIGIN");
    const char *cyo = dataset->GetMetadataItem("CUSTOM_Y_ORIGIN");
    if (cxo != NULL)
        custom_x_origin = std::atof(cxo);
    if (cyo != NULL)
        custom_y_origin = std::atof(cyo);

    GDALRasterBand *band;
    const char *name;
    for (int band_id = 0; band_id < bands.size(); band_id++) {
        band = dataset->GetRasterBand(band_id+1);
        if ( band->GetRasterDataType() != GDT_Float32 )
            std::cerr<<"[warn] only support Float32 bands"<<std::endl;
        band->RasterIO( GF_Read, 0, 0, width, height,
            bands[band_id].data(), width, height, GDT_Float32, 0, 0 );
        name = band->GetMetadataItem("NAME");
        if (name != NULL)
            names[band_id] = name;
    }

    // close properly the dataset
    GDALClose( (GDALDatasetH) dataset );
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
    // get the GDAL GeoTIFF driver
    GDALDriver *driver = GetGDALDriverManager()->GetDriverByName("GTiff");
    if ( driver == NULL )
        throw std::runtime_error("[gdal] could not get the driver");

    std::string tmptif = std::tmpnam(nullptr);
    std::string tmpres = std::tmpnam(nullptr);
    // create the GDAL GeoTiff dataset (n layers of float32)
    GDALDataset *dataset = driver->Create( tmptif.c_str(), width, height,
        1, GDT_Byte, NULL );
    if ( dataset == NULL )
        throw std::runtime_error("[gdal] could not create dataset");

    // set the projection
    OGRSpatialReference spatial_reference;
    char *projection = NULL;

    spatial_reference.SetUTM( utm_zone, utm_north );
    spatial_reference.SetWellKnownGeogCS( "WGS84" );
    spatial_reference.exportToWkt( &projection );
    dataset->SetProjection( projection );
    CPLFree( projection );

    // see GDALDataset::GetGeoTransform()
    dataset->SetGeoTransform( (double *) transform.data() );
    dataset->SetMetadataItem("CUSTOM_X_ORIGIN", std::to_string(custom_x_origin).c_str());
    dataset->SetMetadataItem("CUSTOM_Y_ORIGIN", std::to_string(custom_y_origin).c_str());

    // convert the band from float to byte
    std::vector<uint8_t> band8u = vfloat2vuchar( bands[band] );

    GDALRasterBand *raster_band = dataset->GetRasterBand(1);
    raster_band->RasterIO( GF_Write, 0, 0, width, height,
        (void *) band8u.data(), width, height, GDT_Byte, 0, 0 );
    raster_band->SetMetadataItem("NAME", names[band].c_str());
    // save initial min/max
    auto minmax = std::minmax_element(bands[band].begin(), bands[band].end());
    raster_band->SetMetadataItem("INITIAL_MIN", std::to_string(*minmax.first).c_str());
    raster_band->SetMetadataItem("INITIAL_MAX", std::to_string(*minmax.second).c_str());

    driver = GetGDALDriverManager()->GetDriverByName( driver_shortname.c_str() );
    if ( driver == NULL )
        throw std::runtime_error("[gdal] could not get the driver: " + driver_shortname);

    char ** options = NULL;
    if (!driver_shortname.compare("JPEG"))
        options = CSLSetNameValue( options, "QUALITY", "100" );

    GDALDataset *copy = driver->CreateCopy( tmpres.c_str(), dataset, 0, options,
        NULL, NULL );

    if ( copy != NULL )
        GDALClose( (GDALDatasetH) copy );
    CSLDestroy( options );

    // close properly the dataset
    GDALClose( (GDALDatasetH) dataset );
    std::remove( tmptif.c_str() );
    std::string srcaux = tmpres   + ".aux.xml";
    // might want to check if srcaux exists
    std::string dstaux = filepath + ".aux.xml";
    std::rename( srcaux.c_str(), dstaux.c_str()   );
    std::rename( tmpres.c_str(), filepath.c_str() );
}

} // namespace gdalwrap
