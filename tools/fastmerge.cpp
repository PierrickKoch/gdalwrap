#include <iostream>
#include <gdalwrap/gdal.hpp>

int main(int argc, char * argv[]) {
    if (argc < 4) {
        std::cerr << "usage: " << argv[0] << " file1.tif file2.tif ... out.tif"
                  << std::endl;
        return 1;
    }

    std::vector<gdalwrap::gdal> files(argc - 2);
    for (int filen = 1; filen < argc - 1; filen++) {
        files[filen - 1].load( argv[filen] );
    }
    merge(files).save(argv[argc - 1]);
    return 0;
}
