add_library(mapbox-wagyu::mapbox-wagyu INTERFACE IMPORTED)
set_target_properties(mapbox-wagyu::mapbox-wagyu PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_LIST_DIR}/../../../include"
)
