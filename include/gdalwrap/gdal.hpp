/*
 * gdal.hpp
 *
 * Graph Library for Autonomous and Dynamic Systems
 *
 * author:  Pierrick Koch <pierrick.koch@laas.fr>
 * created: 2013-06-12
 * license: BSD
 */
#ifndef GDAL_HPP
#define GDAL_HPP

#include <map>        // for metadata
#include <string>     // for filepath
#include <vector>     // for raster
#include <array>      // for transform
#include <cmath>      // std::abs
#include <iostream>   // std::{cout,cerr,endl}
#include <algorithm>  // std::minmax
#include <stdexcept>  // std::runtime_error

namespace gdalwrap {

typedef std::vector<float>  raster;
typedef std::vector<raster> rasters;
typedef std::array<double, 2> point_xy_t;
typedef std::array<double, 6> transform_t;
typedef std::vector<std::string> names_t;
typedef std::vector<uint8_t> bytes_t;
typedef std::map<std::string, std::string> metadata_t;

/**
 * get value from a map with default if key does not exist
 */
template <class K, class V>
inline const V& get(const std::map<K, V>& m, const K& k, const V& def) {
    const auto& it = m.find(k);
    if ( it == m.end() )
        return def;
    return it->second;
}

/*
 * gdal : GDALDataset wrapper
 */
class gdal {
    transform_t transform;
    size_t width;   // size x
    size_t height;  // size y
    int  utm_zone;
    bool utm_north;
    double custom_x_origin; // in meters
    double custom_y_origin; // in meters

    void _init();

public:
    rasters bands;
    // band names (band metadata)
    names_t names;
    // dataset metadata (custom origin, and others)
    metadata_t metadata;

    gdal() {
        _init();
    }
    gdal(const gdal& x) {
        copy_impl(x);
    }
    gdal& operator=(const gdal& x) {
        this->clear();
        copy_impl(x);
        return *this;
    }
    void clear() {
        bands.clear();
    }
    void copy_impl(const gdal& x) {
        copy_meta_only(x);
        width = x.width;
        height = x.height;
        bands = x.bands;
        names = x.names;
    }
    gdal(const std::string& filepath) {
        _init();
        load(filepath);
    }

    /** Custom origin (UTM)
     *
     * Store an UTM point in the meta-data.
     */
    void set_custom_origin(double x, double y) {
        custom_x_origin = x;
        custom_y_origin = y;
        metadata["CUSTOM_X_ORIGIN"] = std::to_string(custom_x_origin);
        metadata["CUSTOM_Y_ORIGIN"] = std::to_string(custom_y_origin);
    }

    size_t index_pix(const point_xy_t& p) const {
        return index_pix( std::round(p[0]), std::round(p[1]) );
    }

    size_t index_pix(size_t x, size_t y) const {
        if ( x > width or y > height ) // size_t can not be < 0
            return std::numeric_limits<size_t>::max();
        return x + y * width;
    }

    size_t index_custom(double x, double y) const {
        return index_pix(point_custom2pix(x,y));
    }

    size_t index_utm(double x, double y) const {
        return index_pix(point_utm2pix(x,y));
    }

    point_xy_t point_pix2utm(double x, double y) const {
        point_xy_t p = {x * get_scale_x() + get_utm_pose_x() ,
                        y * get_scale_y() + get_utm_pose_y()};
        return p;
    }

    point_xy_t point_utm2pix(double x, double y) const {
        point_xy_t p = {(x - get_utm_pose_x()) / get_scale_x(),
                        (y - get_utm_pose_y()) / get_scale_y()};
        return p;
    }

    point_xy_t point_pix2custom(double x, double y) const {
        point_xy_t p = point_pix2utm(x, y);
        p[0] -= get_custom_x_origin();
        p[1] -= get_custom_y_origin();
        return p;
    }

    point_xy_t point_custom2pix(double x, double y) const {
        return point_utm2pix( x + get_custom_x_origin(),
                              y + get_custom_y_origin() );
    }

    point_xy_t point_custom2utm(double x, double y) const {
        point_xy_t p = {x + get_custom_x_origin() ,
                        y + get_custom_y_origin() };
        return p;
    }

    point_xy_t point_utm2custom(double x, double y) const {
        point_xy_t p = {x - get_custom_x_origin() ,
                        y - get_custom_y_origin() };
        return p;
    }

    /** Copy meta-data from another instance
     *
     * @param copy another gdal instance
     */
    void copy_meta(const gdal& copy) {
        copy_meta(copy, copy.width, copy.height);
    }
    void copy_meta_only(const gdal& copy) {
        utm_zone  = copy.utm_zone;
        utm_north = copy.utm_north;
        transform = copy.transform;
        metadata  = copy.metadata;
        set_custom_origin(copy.custom_x_origin, copy.custom_y_origin);
    }

    /** Copy meta-data from another instance with different width / height
     *
     * @param copy another gdal instance
     * @param width
     * @param height
     */
    void copy_meta(const gdal& copy, size_t width, size_t height) {
        copy_meta_only(copy);
        names = copy.names;
        set_size(copy.bands.size(), width, height);
    }

    /** Copy meta-data from another instance, except the number/name of layers
     *
     * @param copy another gdal instance
     * @param n_raster number of layers to set (number of rasters)
     */
    void copy_meta(const gdal& copy, size_t n_raster) {
        copy_meta_only(copy);
        set_size(n_raster, copy.width, copy.height);
    }

    /** Set Universal Transverse Mercator projection definition.
     *
     * @param zone UTM zone.
     * @param north TRUE for northern hemisphere, or FALSE for southern.
     */
    void set_utm(int zone, bool north = true) {
        utm_zone = zone;
        utm_north = north;
    }

    /** Set the coefficients for transforming
     * between pixel/line (P,L) raster space,
     * and projection coordinates (Xp,Yp) space.
     *
     * @param pos_x upper left pixel position x
     * @param pos_y upper left pixel position y
     * @param width pixel width (default 1.0)
     * @param height pixel height (default 1.0)
     */
    void set_transform(double pos_x, double pos_y,
            double width = 1.0, double height = 1.0) {
        transform[0] = pos_x;   // top left x
        transform[1] = width;   // w-e pixel resolution
        transform[2] = 0.0;     // rotation, 0 if image is "north up"
        transform[3] = pos_y;   // top left y
        transform[4] = 0.0;     // rotation, 0 if image is "north up"
        transform[5] = height;  // n-s pixel resolution
    }

    /** Set raster size.
     *
     * @param n number of rasters.
     * @param x number of columns.
     * @param y number of rows.
     */
    void set_size(size_t n, size_t x, size_t y) {
        width = x;
        height = y;
        bands.resize( n );
        names.resize( n );
        size_t size = x * y;
        for (auto& band: bands)
            band.resize( size );
    }

    size_t get_width() const {
        return width;
    }

    size_t get_height() const {
        return height;
    }

    /** X scale
     * can be negative if origin is right instead of left.
     */
    double get_scale_x() const {
        return transform[1]; // pixel width
    }

    /** Y scale
     * can be negative if origin is bottom instead of top.
     */
    double get_scale_y() const {
        return transform[5]; // pixel height
    }

    double get_utm_pose_x() const {
        return transform[0]; // upper left pixel position x
    }

    double get_utm_pose_y() const {
        return transform[3]; // upper left pixel position y
    }

    double get_custom_x_origin() const {
        return custom_x_origin;
    }

    double get_custom_y_origin() const {
        return custom_y_origin;
    }

    /** Get a band ID by its name (metadata)
     *
     * @param name Name of the band ID to get.
     * @throws std::out_of_range if name not found.
     */
    size_t get_band_id(const std::string& name) const {
        for (size_t idx = 0; idx < names.size(); idx++)
            if (names[idx] == name)
                return idx;

        throw std::out_of_range("[gdal] band name not found: " + name);
    }

    /** Get a band by its name (metadata)
     *
     * @param name Name of the band to get.
     * @throws std::out_of_range if name not found.
     */
    raster& get_band(const std::string& name) {
        return bands[ get_band_id(name) ];
    }
    const raster& get_band(const std::string& name) const {
        return bands[ get_band_id(name) ];
    }

    const std::string& get_meta(const std::string& key, const std::string& def) const {
        return get(metadata, key, def);
    }

    /** Save as GeoTiff
     *
     * @param filepath path to .tif file.
     */
    void save(const std::string& filepath) const;

    /** Load a GeoTiff
     *
     * @param filepath path to .tif file.
     */
    void load(const std::string& filepath);

    /** Export a band as Byte
     *
     * Distribute the height using `vfloat2vuchar` method.
     * Guess the driver shortname.
     *
     * @param filepath path to .{jpg,gif,png} file.
     * @param band number [0,n-1].
     */
    void export8u(const std::string& filepath, int band) const;

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
    void export8u(const std::string& filepath, int band,
                  const std::string& driver_shortname) const;
};

// helpers

inline bool operator==( const gdal& lhs, const gdal& rhs ) {
    return (lhs.get_width() == rhs.get_width()
        and lhs.get_height() == rhs.get_height()
        and lhs.get_scale_x() == rhs.get_scale_x()
        and lhs.get_scale_y() == rhs.get_scale_y()
        and lhs.get_utm_pose_x() == rhs.get_utm_pose_x()
        and lhs.get_utm_pose_y() == rhs.get_utm_pose_y()
        and lhs.get_custom_x_origin() == rhs.get_custom_x_origin()
        and lhs.get_custom_y_origin() == rhs.get_custom_y_origin()
        and lhs.metadata == rhs.metadata
        and lhs.names == rhs.names
        and lhs.bands == rhs.bands );
}
inline std::ostream& operator<<(std::ostream& os, const gdal& value) {
    return os<<"GDAL["<<value.get_width()<<","<<value.get_height()<<"]";
}

/** handy method to display a raster
 *
 * @param v vector of float
 * @returns vector of uint8_t (unisgned char)
 *
 * distribute as:
 *   min(v) -> 0
 *   max(v) -> 255
 */
inline bytes_t raster2bytes(const raster& v) {
    auto minmax = std::minmax_element(v.begin(), v.end());
    float min = *minmax.first;
    float max = *minmax.second;
    float diff = max - min;
    bytes_t b(v.size());
    if (diff == 0) // max == min (useless band)
        return b;

    float coef = 255.0 / diff;
    std::transform(v.begin(), v.end(), b.begin(),
        // C++11 lambda, capture local varibles by reference [&]
        [&](float f) -> uint8_t { std::floor( coef * (f - min) ); });

    return b;
}
inline bytes_t vfloat2vuchar(const raster& v) {
    return raster2bytes(v);
}

inline std::string toupper(const std::string& in) {
    std::string up(in);
    std::transform(up.begin(), up.end(), up.begin(), std::ptr_fun<int, int>(std::toupper));
    return up;
}

} // namespace gdalwrap

#endif // GDAL_HPP

