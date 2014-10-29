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

static const uint nloop = 1000;
static const uint nband = 8;
static const uint nsx   = 400;
static const uint nsy   = 400;
static const std::string name = std::tmpnam(nullptr);

std::ifstream::pos_type filesize(const std::string& filename)
{
    std::ifstream in(filename, std::ifstream::ate | std::ifstream::binary);
    return in.tellg();
}

void stats(const gdalwrap::gdal& geotif, bool compress) {
    std::chrono::time_point<std::chrono::system_clock> start, end;
    std::chrono::duration<double> elapsed_seconds;
    start = std::chrono::system_clock::now();
    for (uint i = 0; i < nloop; i++) {
        geotif.save(name, compress);
    }
    end = std::chrono::system_clock::now();
    elapsed_seconds = end-start;
    std::cout << "gdal::save (x" << nloop << "):  " << elapsed_seconds.count() << "s\n";
    std::cout << "gdal::save filesize: " << filesize(name) / 1024.0 << " kB\n";
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
    std::cout << "half random\n";
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

    std::remove( name.c_str() );
    std::cout << "done." << std::endl;
    return 0;
}


/* result on my setup (i7QM):
empty
compress: off
gdal::save (x1000):  16.0366s
gdal::save filesize: 5004.08 kB
compress: on
gdal::save (x1000):  17.6654s
gdal::save filesize: 34.9512 kB
half random
compress: off
gdal::save (x1000):  16.0599s
gdal::save filesize: 5004.08 kB
compress: on
gdal::save (x1000):  41.8193s
gdal::save filesize: 1152.28 kB
full
compress: off
gdal::save (x1000):  15.7392s
gdal::save filesize: 5004.08 kB
compress: on
gdal::save (x1000):  116.127s
gdal::save filesize: 4505.44 kB

*/
