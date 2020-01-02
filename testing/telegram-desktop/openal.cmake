add_library(external_openal INTERFACE IMPORTED GLOBAL)
add_library(desktop-app::external_openal ALIAS external_openal)

find_package(OpenAL REQUIRED)

target_include_directories(external_openal SYSTEM INTERFACE ${OPENAL_INCLUDE_DIR})
target_link_libraries(external_openal INTERFACE ${OPENAL_LIBRARY})
