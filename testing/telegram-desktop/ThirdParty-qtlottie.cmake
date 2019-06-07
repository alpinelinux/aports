project(qtlottie)

find_package(Qt5 REQUIRED COMPONENTS Core Widgets Gui)

foreach(__qt_module IN ITEMS QtCore QtGui)
	list(APPEND QT_PRIVATE_INCLUDE_DIRS
		${QT_INCLUDE_DIR}/${__qt_module}/${Qt5_VERSION}
		${QT_INCLUDE_DIR}/${__qt_module}/${Qt5_VERSION}/${__qt_module}
	)
endforeach()
message(STATUS "Using Qt private include directories: ${QT_PRIVATE_INCLUDE_DIRS}")

file(GLOB QTLOTTIE_SOURCE_FILES
	src/bodymovin/*.cpp
	src/imports/rasterrenderer/*.cpp
)

add_library(${PROJECT_NAME} STATIC ${QTLOTTIE_SOURCE_FILES})

target_include_directories(${PROJECT_NAME} PUBLIC src src/bodymovin ${QT_PRIVATE_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} Qt5::Core Qt5::Widgets Qt5::Gui)
