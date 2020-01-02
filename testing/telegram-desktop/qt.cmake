add_library(external_qt INTERFACE IMPORTED GLOBAL)
add_library(desktop-app::external_qt ALIAS external_qt)

find_package(Qt5 REQUIRED COMPONENTS Core DBus Gui Widgets Network)
get_target_property(QTCORE_INCLUDE_DIRS Qt5::Core INTERFACE_INCLUDE_DIRECTORIES)
list(GET QTCORE_INCLUDE_DIRS 0 QT_INCLUDE_DIR)

foreach(__qt_module IN ITEMS QtCore QtGui)
	list(APPEND QT_PRIVATE_INCLUDE_DIRS
		${QT_INCLUDE_DIR}/${__qt_module}/${Qt5_VERSION}
		${QT_INCLUDE_DIR}/${__qt_module}/${Qt5_VERSION}/${__qt_module}
	)
endforeach()
message(STATUS "Using Qt private include directories: ${QT_PRIVATE_INCLUDE_DIRS}")

if (LINUX)
	if (NOT build_linux32)
		target_compile_definitions(external_qt INTERFACE Q_OS_LINUX64)
	else()
		target_compile_definitions(external_qt INTERFACE Q_OS_LINUX32)
	endif()
endif()

target_compile_definitions(external_qt
INTERFACE
	_REENTRANT
	QT_PLUGIN
	QT_WIDGETS_LIB
	QT_NETWORK_LIB
	QT_GUI_LIB
	QT_CORE_LIB
)

target_include_directories(external_qt SYSTEM
INTERFACE
	${QT_PRIVATE_INCLUDE_DIRS}
)
target_link_libraries(external_qt
INTERFACE
	Qt5::DBus
	Qt5::Network
	Qt5::Widgets
)
