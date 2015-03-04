#undef NDEBUG
#include <cassert>
#include <iostream>
#include <chrono>
#include <ctime>
#include <cstdio>
#include <string>
#include <fstream>
#include <cstdlib> // std::rand
#include <gdalwrap/gdal.hpp>

static const uint nloop = 100;
static const uint nband = 8;
static const uint nsx   = 400;
static const uint nsy   = 400;

std::ifstream::pos_type filesize(const std::string& filename) {
    std::ifstream in(filename, std::ifstream::ate | std::ifstream::binary);
    return in.tellg();
}

void stats(const gdalwrap::gdal& geotif, bool compress) {
    std::chrono::time_point<std::chrono::system_clock> start, end;
    std::chrono::duration<double> elapsed_seconds;
    std::string name;
    double size = 0;
    start = std::chrono::system_clock::now();
    for (uint i = 0; i < nloop; i++) {
        name = std::tmpnam(nullptr);
        if (compress)
            geotif.save(name, "GTiff", gdalwrap::compress);
        else
            geotif.save(name);
        size += filesize(name) / 1024.0;
        std::remove( name.c_str() );
    }
    end = std::chrono::system_clock::now();
    elapsed_seconds = end-start;
    std::cout << "gdal::save (x" << nloop << "):  " << elapsed_seconds.count() << "s\n";
    std::cout << "gdal::save filesize: " << size / nloop << " kB\n";
}

void randomize(gdalwrap::gdal& geotif, size_t band,
        size_t width, size_t height, double scale = 1000.0) {
    scale /= RAND_MAX;
    for (size_t b = 0; b < band; b++) {
        for (size_t i = 0; i < width; i++) {
            for (size_t j = 0; j < height; j++) {
                geotif.bands[b][i + j * width] = scale * std::rand();
            }
        }
    }
}

int main(int argc, char * argv[]) {
    std::cout << "gdalwrap io test..." << std::endl;

    gdalwrap::gdal geotif;
    geotif.set_size(nband, nsx, nsy);

    std::cout << "empty\n";
    std::cout << "compress: off\n";
    stats(geotif, false);
    std::cout << "compress: on\n";
    stats(geotif, true);

    randomize(geotif, nband, nsx/2, nsy/2);
    std::cout << "25% random\n";
    std::cout << "compress: off\n";
    stats(geotif, false);
    std::cout << "compress: on\n";
    stats(geotif, true);

    randomize(geotif, nband, nsx, nsy);
    std::cout << "full\n";
    std::cout << "compress: off\n";
    stats(geotif, false);
    std::cout << "compress: on\n";
    stats(geotif, true);

    std::cout << "done." << std::endl;
    return 0;
}


/* result on my setup (i7QM):
gdalwrap io test...
empty
compress: off
gdal::save (x1000):  15.6806s
gdal::save filesize: 5004.08 kB
compress: on
gdal::save (x1000):  17.6593s
gdal::save filesize: 34.9512 kB
25% random
compress: off
gdal::save (x1000):  15.6709s
gdal::save filesize: 5004.08 kB
compress: on
gdal::save (x1000):  42.1742s
gdal::save filesize: 1152.28 kB
full
compress: off
gdal::save (x1000):  15.7503s
gdal::save filesize: 5004.08 kB
compress: on
gdal::save (x1000):  115.614s
gdal::save filesize: 4505.44 kB
done.
*/
