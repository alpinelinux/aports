add_library(external_opus INTERFACE IMPORTED GLOBAL)
add_library(desktop-app::external_opus ALIAS external_opus)

find_package(PkgConfig REQUIRED)
pkg_check_modules(OPUS REQUIRED opus)

target_include_directories(external_opus SYSTEM INTERFACE ${OPUS_INCLUDE_DIRS})
target_link_libraries(external_opus INTERFACE ${OPUS_LIBRARIES})
