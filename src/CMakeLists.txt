file(GLOB gdalwrap_SRCS "*.cpp")
add_library( gdalwrap SHARED ${gdalwrap_SRCS} )
target_link_libraries( gdalwrap ${GDAL_LIBRARIES} )
install(TARGETS gdalwrap DESTINATION lib)
install_pkg_config_file(gdalwrap
    DESCRIPTION "C++11 GDAL wrapper"
    CFLAGS
    LIBS -lgdalwrap
    VERSION ${PACKAGE_VERSION})
