add_library(external_zlib INTERFACE IMPORTED GLOBAL)
add_library(desktop-app::external_zlib ALIAS external_zlib)

find_package(PkgConfig REQUIRED)
find_package(ZLIB REQUIRED)
pkg_check_modules(MINIZIP REQUIRED minizip)

target_include_directories(external_zlib SYSTEM INTERFACE ${ZLIB_INCLUDE_DIR} ${MINIZIP_INCLUDE_DIRS})
target_link_libraries (external_zlib INTERFACE ${ZLIB_LIBRARY_RELEASE} ${MINIZIP_LIBRARIES})
