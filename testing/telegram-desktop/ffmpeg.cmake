# This file is part of Desktop App Toolkit,
# a set of libraries for developing nice desktop applications.
#
# For license and copyright information please follow this link:
# https://github.com/desktop-app/legal/blob/master/LEGAL

add_library(external_ffmpeg INTERFACE IMPORTED GLOBAL)
add_library(desktop-app::external_ffmpeg ALIAS external_ffmpeg)

find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG REQUIRED libavcodec libavformat libavutil libswresample libswscale)

target_include_directories(external_ffmpeg SYSTEM INTERFACE ${FFMPEG_INCLUDE_DIR})
target_link_libraries(external_ffmpeg INTERFACE ${FFMPEG_LIBRARIES})
