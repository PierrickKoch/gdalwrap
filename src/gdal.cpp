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

using std::string;

template <typename T>
GDALDataType data_type() { return GDT_Unknown; }
// explicit (full) template specialization
template <> GDALDataType data_type<float>() { return GDT_Float32; }
template <> GDALDataType data_type<double>() { return GDT_Float64; }
template <> GDALDataType data_type<int8_t>() { return GDT_Byte; }
template <> GDALDataType data_type<uint8_t>() { return GDT_Byte; }
template <> GDALDataType data_type<int16_t>() { return GDT_Int16; }
template <> GDALDataType data_type<int32_t>() { return GDT_Int32; }
template <> GDALDataType data_type<uint16_t>() { return GDT_UInt16; }
template <> GDALDataType data_type<uint32_t>() { return GDT_UInt32; }

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

void _register() {
    // Register all known configured GDAL drivers.
    GDALAllRegister();
}

/** Save as GeoTiff
 *
 * @param filepath path to .tif file.
 * @param compress fastest deflate (zlib/png).
 */
template void gdal_base<float>::save(const string&, const string&, const options_t&) const;
template void gdal_base<double>::save(const string&, const string&, const options_t&) const;
template void gdal_base<uint8_t>::save(const string&, const string&, const options_t&) const;
template void gdal_base<uint32_t>::save(const string&, const string&, const options_t&) const;
template <typename T>
void gdal_base<T>::save(const string& filepath,
                   const string& driver_shortname,
                   const options_t& options) const {
    // get the GDAL GeoTIFF driver
    GDALDriver *driver = GetGDALDriverManager()->GetDriverByName(
        driver_shortname.c_str() );
    if ( driver == NULL )
        throw std::runtime_error("[gdal] could not get the driver");

    char ** c_opts = NULL;
    for (const auto& pair : options)
        c_opts = CSLSetNameValue(c_opts, pair.first.c_str(), pair.second.c_str());

    // create the GDAL GeoTiff dataset (n layers of float32)
    GDALDataset *dataset = driver->Create( filepath.c_str(), width, height,
        bands.size(), data_type<T>(), c_opts );
    if ( dataset == NULL )
        throw std::runtime_error("[gdal] could not create the dataset");

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
            (void *) bands[band_id].data(), width, height, data_type<T>(), 0, 0 );
        for (const auto& pair : band_metadata[band_id])
            band->SetMetadataItem( pair.first.c_str(), pair.second.c_str() );
        // XXX band->SetNoDataValue(NO_DATA_VALUE);
    }

    // close properly the dataset
    GDALClose( (GDALDatasetH) dataset );
    CSLDestroy( c_opts );
}

/** Load a GeoTiff
 *
 * @param filepath path to .tif file.
 */
template void gdal_base<float>::load(const string&);
template void gdal_base<double>::load(const string&);
template void gdal_base<uint8_t>::load(const string&);
template void gdal_base<uint32_t>::load(const string&);
template <typename T>
void gdal_base<T>::load(const string& filepath) {
    // Open a raster file as a GDALDataset.
    GDALDataset *dataset = (GDALDataset *) GDALOpen(filepath.c_str(), GA_ReadOnly);
    if ( dataset == NULL )
        throw std::runtime_error("[gdal] could not open the given filepath");

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
    metadata = get_metadata(dataset->GetMetadata());

    GDALRasterBand *band;
    for (size_t band_id = 0; band_id < bands.size(); band_id++) {
        band = dataset->GetRasterBand(band_id+1);
        if ( band->GetRasterDataType() != data_type<T>() )
            std::cerr<<"[warn]["<< __func__ <<"] data type missmatch"<<std::endl;
        band->RasterIO( GF_Read, 0, 0, width, height,
            bands[band_id].data(), width, height, data_type<T>(), 0, 0 );
        band_metadata[band_id] = get_metadata(band->GetMetadata());
    }

    // close properly the dataset
    GDALClose( (GDALDatasetH) dataset );
}

/** Export a band as Byte
 *
 * First create a temporary GeoTiff file with the Byte band,
 * and then copy it to the `filepath` with the correct driver.
 * Because `Create` is not supported by all driver, but `CreateCopy` is.
 *
 * @param filepath path to .{jpg,gif,png} file.
 * @param band8u the band to save, vector<uint8>.
 * @param driver_shortname see http://gdal.org/formats_list.html
 */
template <typename T>
void gdal_base<T>::export8u(const string& filepath, std::vector<bytes_t> band8u,
                       const string& driver_shortname) const {
    // get the driver from its shortname
    GDALDriver *driver = GetGDALDriverManager()->GetDriverByName(
        driver_shortname.c_str() );

    if ( driver == NULL )
        throw std::runtime_error("[gdal] could not get the driver: " +
            driver_shortname);
    // get the GDAL GeoTIFF driver
    GDALDriver *drtiff = GetGDALDriverManager()->GetDriverByName("GTiff");
    if ( drtiff == NULL )
        throw std::runtime_error("[gdal] could not get the GTiff driver");

    // could use something like tempnam(dirname(filepath), NULL)
    // but it does not garantee the result to be local, if TMPDIR is set.
    // and std::rename(2) works only locally, not across disks.
    string tmptif = filepath + ".tif.export8u.tmp";
    string tmpres = filepath +     ".export8u.tmp";
    // create the GDAL GeoTiff dataset (1 layers of byte)
    GDALDataset *dataset = drtiff->Create( tmptif.c_str(), width, height,
        band8u.size(), GDT_Byte, NULL );
    if ( dataset == NULL )
        throw std::runtime_error("[gdal] could not create dataset");

    set_wgs84(dataset, utm_zone, utm_north);
    // see GDALDataset::GetGeoTransform()
    dataset->SetGeoTransform( (double *) transform.data() );
    // set dataset metadata
    for (const auto& pair : metadata)
        dataset->SetMetadataItem( pair.first.c_str(), pair.second.c_str() );

    GDALRasterBand *band;
    for (size_t band_id = 0; band_id < band8u.size(); band_id++) {
        band = dataset->GetRasterBand(band_id+1);
        band->RasterIO( GF_Write, 0, 0, width, height,
            (void *) band8u[band_id].data(), width, height, GDT_Byte, 0, 0 );
    }

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
    string srcaux = tmpres   + ".aux.xml";
    string dstaux = filepath + ".aux.xml";
    std::rename( srcaux.c_str(), dstaux.c_str()   );
    std::rename( tmpres.c_str(), filepath.c_str() );
}

} // namespace gdalwrap
