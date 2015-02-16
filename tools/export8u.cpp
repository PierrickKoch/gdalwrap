#include <iostream>
#include <gdalwrap/gdal.hpp>

int main(int argc, char * argv[]) {
    if (argc < 4) {
        std::cerr << "usage: " << argv[0] << " file.tif band file.gif" << std::endl;
        return 1;
    }
    gdalwrap::gdal geotiff(argv[1]);
    geotiff.export8u(argv[3], std::atoi(argv[2]));

    return 0;
}
