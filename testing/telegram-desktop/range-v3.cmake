add_library(external_ranges INTERFACE IMPORTED GLOBAL)
add_library(desktop-app::external_ranges ALIAS external_ranges)

find_package(range-v3 REQUIRED)

target_include_directories(external_ranges SYSTEM
INTERFACE
	${RANGE_V3_INCLUDE_DIRS}
)
