add_library(external_breakpad INTERFACE IMPORTED GLOBAL)
add_library(desktop-app::external_breakpad ALIAS external_breakpad)

target_include_directories(external_breakpad SYSTEM
INTERFACE
    ${third_party_loc}/breakpad/src
)
