add_library(external_lz4 INTERFACE IMPORTED GLOBAL)
add_library(desktop-app::external_lz4 ALIAS external_lz4)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LZ4 REQUIRED liblz4)

target_link_libraries(external_lz4 INTERFACE ${LZ4_LIBRARIES})
