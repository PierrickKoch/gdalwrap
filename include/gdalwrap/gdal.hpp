/*
 * gdal.hpp
 *
 * C++11 GDAL wrapper
 *
 * author:  Pierrick Koch <pierrick.koch@laas.fr>
 * created: 2013-06-12
 * license: BSD
 */
#ifndef GDAL_HPP
#define GDAL_HPP

#include <map>        // for metadata
#include <array>      // for transform
#include <cmath>      // std::abs
#include <string>     // for filepath
#include <vector>     // for raster
#include <cassert>    // for assert
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
typedef std::map<std::string, std::string> options_t;

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

void _register();

inline std::string toupper(const std::string& in) {
    std::string up(in);
    std::transform(up.begin(), up.end(), up.begin(), std::ptr_fun<int, int>(std::toupper));
    return up;
}

/**
 * works for JPEG, PNG, TIFF, GIF and others,
 * see http://www.gdal.org/formats_list.html
 */
inline std::string driver_name(const std::string& filepath) {
    std::string ext = toupper( filepath.substr( filepath.rfind(".") + 1 ) );
    if (!ext.compare("JPG")) {
        ext = "JPEG";
    } else if (!ext.compare("TIF")) {
        ext = "GTiff";
    }
    return ext;
}

inline void fill_metadata(char * const* c_metadata, metadata_t& metadata) {
    if (!c_metadata)
        return;
    for (size_t meta_id = 0; c_metadata[meta_id] != NULL; meta_id++) {
        std::string item( c_metadata[meta_id] );
        std::string::size_type n = item.find('=');
        // k,v = item.split('=')
        metadata[ item.substr(0, n) ] = item.substr(n + 1);
    }
}
inline metadata_t get_metadata(char * const* c_metadata) {
    metadata_t metadata;
    fill_metadata(c_metadata, metadata);
    return metadata;
}

/** GDALDataset wrapper
 *
 * This class offers I/O for GDAL Float32 GeoTiff with metadata support.
 * It stores data using C++11 STL containers (std::{vector,map,array}).
 */
template <typename T>
class gdal_base {
    transform_t transform;
    size_t width;   // size x
    size_t height;  // size y
    int  utm_zone;
    bool utm_north;

protected:
    void _init() {
        _register();
        set_transform(0, 0);
        set_utm(0);
    }

public:
    std::vector<std::vector<T>> bands;
    std::vector<metadata_t> band_metadata;
    // dataset metadata (custom origin, and others)
    metadata_t metadata;

    gdal_base() {
        this->_init();
    }
    gdal_base(const gdal_base& x) {
        this->copy_impl(x);
    }
    gdal_base(const std::string& filepath) {
        this->_init();
        this->load(filepath);
    }
    gdal_base& operator=(const gdal_base& x) {
        this->clear();
        this->copy_impl(x);
        return *this;
    }
    void clear() {
        this->bands.clear();
    }
    void copy_impl(const gdal_base& x) {
        this->copy_meta_only(x);
        this->width = x.width;
        this->height = x.height;
        this->bands = x.bands;
        this->band_metadata = x.band_metadata;
    }

    size_t index_pix(const point_xy_t& p) const {
        return index_pix( std::round(p[0]), std::round(p[1]) );
    }

    size_t index_pix(size_t x, size_t y) const {
        if ( x >= width or y >= height ) // size_t can not be < 0
            return std::numeric_limits<size_t>::max();
        return x + y * width;
    }

    size_t index_utm(double x, double y) const {
        return index_pix(point_utm2pix(x,y));
    }

    point_xy_t point_pix2utm(double x, double y) const {
        point_xy_t p = {{x * get_scale_x() + get_utm_pose_x() ,
                         y * get_scale_y() + get_utm_pose_y() }};
        return p;
    }

    point_xy_t point_utm2pix(double x, double y) const {
        point_xy_t p = {{(x - get_utm_pose_x()) / get_scale_x() ,
                         (y - get_utm_pose_y()) / get_scale_y() }};
        return p;
    }

    /** Copy meta-data from another instance
     *
     * @param copy another gdal instance
     */
    void copy_meta(const gdal_base& copy) {
        copy_meta(copy, copy.width, copy.height);
    }
    void copy_meta_only(const gdal_base& copy) {
        utm_zone  = copy.utm_zone;
        utm_north = copy.utm_north;
        transform = copy.transform;
        metadata  = copy.metadata;
    }

    /** Copy meta-data from another instance with different width / height
     *
     * @param copy another gdal instance
     * @param width
     * @param height
     */
    void copy_meta(const gdal_base& copy, size_t width, size_t height) {
        copy_meta_only(copy);
        set_size(copy.bands.size(), width, height);
    }

    /** Copy meta-data from another instance, except the number/name of layers
     *
     * @param copy another gdal instance
     * @param n_raster number of layers to set (number of rasters)
     */
    void copy_meta(const gdal_base& copy, size_t n_raster) {
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
    void set_size(size_t n, size_t x, size_t y, T no_data = 0) {
        width = x;
        height = y;
        bands.resize( n );
        band_metadata.resize( n );
        size_t size = x * y;
        for (auto& band: bands)
            band.resize( size, no_data );
    }
    /** Set meta size. Does not change the container (unsafe).
     *
     * @param x number of columns.
     * @param y number of rows.
     */
    void set_size(size_t x, size_t y) {
        width = x;
        height = y;
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

    const std::string& get_meta(const std::string& key, const std::string& def) const {
        return get(metadata, key, def);
    }

    const std::string& get_band_meta(size_t band_id, const std::string& key, const std::string& def) const {
        return get(band_metadata[band_id], key, def);
    }

    /** Save as GeoTiff
     *
     * @param filepath path to .tif file.
     */
    void save(const std::string& filepath,
              const std::string& driver_shortname,
              const options_t& opt) const;
    void save(const std::string& filepath,
              const std::string& driver_shortname) const {
        return this->save(filepath, driver_shortname, {});
    }
    void save(const std::string& filepath) const {
        return this->save(filepath, "GTiff", {});
    }


    /** Load a GeoTiff
     *
     * @param filepath path to .tif file.
     */
    void load(const std::string& filepath);

    /** Export a band as Byte
     *
     * Distribute the height using `raster2bytes` method.
     * Guess the driver shortname.
     *
     * @param filepath path to .{jpg,gif,png} file.
     * @param band number [0,n-1].
     */
    void export8u(const std::string& filepath, int band) const {
        // convert the band from float to byte
        export8u(filepath, { raster2bytes(bands[band]) }, driver_name(filepath));
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
    void export8u(const std::string& filepath, std::vector<bytes_t> band8u,
                  const std::string& driver_shortname) const;
};

template<typename T>
class gdal_named : public gdal_base<T> {
public:
    void set_band_name(size_t band_id, const std::string& name) {
        this->band_metadata[band_id]["NAME"] = name;
    }
    std::string get_band_name(size_t band_id) {
        return this->band_metadata[band_id]["NAME"];
    }

    /** Get a band ID by its name (metadata)
     *
     * @param name Name of the band ID to get.
     * @throws std::out_of_range if name not found.
     */
    size_t get_band_id(const std::string& name) const {
        size_t band_id = 0;
        for (const auto& bm : this->band_metadata) {
            if (!name.compare(bm.second)) {
                return band_id;
            }
            band_id++;
        }
        throw std::out_of_range("[gdal] band name not found: " + name);
    }

    /** Get a band by its name (metadata)
     *
     * @param name Name of the band to get.
     * @throws std::out_of_range if name not found.
     */
    raster& get_band(const std::string& name) {
        return this->bands[ get_band_id(name) ];
    }
    const raster& get_band(const std::string& name) const {
        return this->bands[ get_band_id(name) ];
    }
};

template<typename T>
class gdal_custom : public gdal_named<T> {
    double custom_x_origin; // in meters
    double custom_y_origin; // in meters
    double custom_z_origin; // in meters

    void _init() {
        gdal_named<T>::_init();
        set_custom_origin(0, 0, 0);
    }
public:
    gdal_custom() {
        this->_init();
    }
    gdal_custom(const std::string& filepath) {
        this->_init();
        this->load(filepath);
    }
    void load(const std::string& filepath) {
        gdal_named<T>::load(filepath);
        // WARN std::stod might throw std::invalid_argument
        custom_x_origin = std::stod( this->get_meta("CUSTOM_X_ORIGIN", "0") );
        custom_y_origin = std::stod( this->get_meta("CUSTOM_Y_ORIGIN", "0") );
        custom_z_origin = std::stod( this->get_meta("CUSTOM_Z_ORIGIN", "0") );
    }

    /** Custom origin (UTM)
     *
     * Store an UTM point in the meta-data.
     */
    void set_custom_origin(double x, double y, double z = 0.0) {
        custom_x_origin = x;
        custom_y_origin = y;
        custom_z_origin = z;
        this->metadata["CUSTOM_X_ORIGIN"] = std::to_string(custom_x_origin);
        this->metadata["CUSTOM_Y_ORIGIN"] = std::to_string(custom_y_origin);
        this->metadata["CUSTOM_Z_ORIGIN"] = std::to_string(custom_z_origin);
    }

    void copy_meta_only(const gdal_custom& copy) {
        gdal_named<T>::copy_meta_only(copy);
        set_custom_origin(copy.custom_x_origin, copy.custom_y_origin,
            copy.custom_z_origin);
    }

    size_t index_custom(double x, double y) const {
        return this->index_pix(point_custom2pix(x,y));
    }

    point_xy_t point_pix2custom(double x, double y) const {
        point_xy_t p = this->point_pix2utm(x, y);
        p[0] -= get_custom_x_origin();
        p[1] -= get_custom_y_origin();
        return p;
    }

    point_xy_t point_custom2pix(double x, double y) const {
        return this->point_utm2pix( x + get_custom_x_origin() ,
                              y + get_custom_y_origin() );
    }

    point_xy_t point_custom2utm(double x, double y) const {
        point_xy_t p = {{x + get_custom_x_origin() ,
                         y + get_custom_y_origin() }};
        return p;
    }

    point_xy_t point_utm2custom(double x, double y) const {
        point_xy_t p = {{x - get_custom_x_origin() ,
                         y - get_custom_y_origin() }};
        return p;
    }

    double get_custom_x_origin() const {
        return custom_x_origin;
    }

    double get_custom_y_origin() const {
        return custom_y_origin;
    }

    double get_custom_z_origin() const {
        return custom_z_origin;
    }
};

// helpers
template <typename T>
inline bool operator==( const gdal_base<T>& lhs, const gdal_base<T>& rhs ) {
    return (lhs.get_width() == rhs.get_width()
        and lhs.get_height() == rhs.get_height()
        and lhs.get_scale_x() == rhs.get_scale_x()
        and lhs.get_scale_y() == rhs.get_scale_y()
        and lhs.get_utm_pose_x() == rhs.get_utm_pose_x()
        and lhs.get_utm_pose_y() == rhs.get_utm_pose_y()
        and lhs.metadata == rhs.metadata
        and lhs.bands == rhs.bands );
}
template <typename T>
inline std::ostream& operator<<(std::ostream& os, const gdal_base<T>& value) {
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
template <typename T>
inline bytes_t raster2bytes(const std::vector<T>& v) {
    auto minmax = std::minmax_element(v.begin(), v.end());
    T min = *minmax.first;
    T max = *minmax.second;
    T diff = max - min;
    bytes_t b(v.size());
    if (diff == 0) // max == min (useless band)
        return b;

    T coef = 255.0 / diff;
    std::transform(v.begin(), v.end(), b.begin(),
        // C++11 lambda, capture local varibles by reference [&]
        [&](T f) -> uint8_t { return std::floor( coef * (f - min) ); });

    return b;
}
/**
 * normalize [0, 1.0] in place
 */
inline raster normalize(raster& v) {
    auto minmax = std::minmax_element(v.begin(), v.end());
    float min = *minmax.first;
    float max = *minmax.second;
    float diff = max - min;
    if (diff == 0) // max == min
        return v;
    // normalize in place
    for (auto& f : v)
        f = (f - min) / diff;
    return v;
}

template <typename T>
gdal_base<T> merge(const std::vector<gdal_base<T>>& files, T no_data = 0);

template class gdal_custom<float>;
typedef gdal_custom<float> gdal;

static const options_t compress = {
    {"COMPRESS", "DEFLATE"},
    {"PREDICTOR", "1"},
    {"ZLEVEL", "1"}
};

} // namespace gdalwrap

#endif // GDAL_HPP
