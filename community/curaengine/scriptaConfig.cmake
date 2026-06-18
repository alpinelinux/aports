add_library(scripta::scripta INTERFACE IMPORTED)
set_target_properties(scripta::scripta PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_LIST_DIR}/../../../include"
)
