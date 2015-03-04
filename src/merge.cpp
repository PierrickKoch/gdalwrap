/*
 * merge.cpp
 *
 * C++11 GDAL wrapper
 *
 * author:  Pierrick Koch <pierrick.koch@laas.fr>
 * created: 2015-02-16
 * license: BSD
 */

#include <cmath>
#include <limits>
#include <string>
#include <stdexcept>        // for runtime_error
#include <gdalwrap/gdal.hpp>

namespace gdalwrap {

bool same(double a, double b) {
    return std::abs(a - b) < std::numeric_limits<double>::epsilon();
}

template <typename T>
gdal<T> merge(const std::vector<gdal<T>>& files, T no_data = 0) {
    double scale_x, scale_y, utm_x, utm_y,
           min_utm_x, max_utm_x,
           min_utm_y, max_utm_y;
    size_t width, height, bsize;
    // init
    scale_x = files[0].get_scale_x();
    scale_y = files[0].get_scale_y();
    width = files[0].get_width();
    height = files[0].get_height();
    bsize = files[0].bands.size();
    min_utm_x = max_utm_x = files[0].get_utm_pose_x();
    min_utm_y = max_utm_y = files[0].get_utm_pose_y();
    // get min/max
    for (const auto& file : files) {
        if (same(scale_x, file.get_scale_x()) and
            same(scale_y, file.get_scale_y()) and
            same(width, file.get_width()) and
            same(height, file.get_height()) and
            bsize == file.bands.size() ) {
            // get min/max
            utm_x = file.get_utm_pose_x();
            utm_y = file.get_utm_pose_y();
            if (utm_x < min_utm_x) min_utm_x = utm_x;
            if (utm_y < min_utm_y) min_utm_y = utm_y;
            if (utm_x > max_utm_x) max_utm_x = utm_x;
            if (utm_y > max_utm_y) max_utm_y = utm_y;
        } else {
            throw std::runtime_error("file size missmatch");
        }
    }
    // setup resulting container where we'll copy all tiles
    double ulx = min_utm_x, lrx = max_utm_x + scale_x * width,
           uly = max_utm_y, lry = min_utm_y + scale_y * height;
    size_t sx = std::floor((lrx - ulx) / scale_x + 0.5),
           sy = std::floor((lry - uly) / scale_y + 0.5);
    gdal<T> result;
    result.copy_meta_only(files[0]);
    result.names = files[0].names;
    result.set_transform(ulx, uly, scale_x, scale_y);
    result.set_size(bsize, sx, sy, no_data);

    for (const auto& file : files) {
        utm_x = file.get_utm_pose_x();
        utm_y = file.get_utm_pose_y();
        int xoff = std::floor( (utm_x - ulx) / scale_x + 0.1 );
        int yoff = std::floor( (utm_y - uly) / scale_y + 0.1 );
        size_t start = xoff + yoff * sx;
        for (int band = 0; band < bsize; band++) {
            // copy file.bands[band] into result.bands[band]
            auto it2 = result.bands[band].begin() + start;
            for (auto it1 = file.bands[band].begin();
                it1 < file.bands[band].end();
                it1 += width, it2 += sx) {
                std::copy(it1, it1+width, it2);
            }
        }
    }

    return result;
}

} // namespace gdalwrap
