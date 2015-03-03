#include <string>
#include <vector>
#include <iostream>         // cout,cerr,endl
#include <stdexcept>        // for runtime_error
#include <gdal_priv.h>      // for GDALDataset
#include <ogr_spatialref.h> // for OGRSpatialReference
#include <cpl_string.h>     // for CSLSetNameValue

void save(const std::string& filepath) {
    const size_t width = 640, height = 480;
    std::vector<float> bf32(640*480);
    std::vector<uint32_t> bi32(640*480);
    // get the GDAL GeoTIFF driver
    GDALDriver *driver = GetGDALDriverManager()->GetDriverByName("GTiff");
    if ( driver == NULL )
        throw std::runtime_error("[gdal] could not get the driver");

    char ** options = NULL;
    // create the GDAL GeoTiff dataset (n layers of float32)
    GDALDataset *dataset = driver->Create( filepath.c_str(), width, height,
        2, GDT_Float32, options );
    if ( dataset == NULL )
        throw std::runtime_error("[gdal] could not create (multi-layers float32)");

    GDALRasterBand *band;
    band = dataset->GetRasterBand(1);
    band->RasterIO( GF_Write, 0, 0, width, height,
        (void *) bf32.data(), width, height, GDT_Float32, 0, 0 );
    band = dataset->GetRasterBand(2);
    band->RasterIO( GF_Write, 0, 0, width, height,
        (void *) bi32.data(), width, height, GDT_Int32, 0, 0 );

    // close properly the dataset
    GDALClose( (GDALDatasetH) dataset );
    CSLDestroy( options );
}

int main(int argc, char * argv[]) {
    // Register all known configured GDAL drivers.
    GDALAllRegister();
    save("test.tif");
    return 0;
}
