include_directories(.)

macro(add_gdalwrap_test name)
    add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} gdalwrap)
    add_test(${name} ${name})
endmacro()

add_gdalwrap_test( io_test )
