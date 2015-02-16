add_executable(gdal_export8u export8u.cpp)
target_link_libraries(gdal_export8u gdalwrap)
install(TARGETS gdal_export8u DESTINATION bin)

add_executable(gdal_fastmerge fastmerge.cpp)
target_link_libraries(gdal_fastmerge gdalwrap)
install(TARGETS gdal_fastmerge DESTINATION bin)
