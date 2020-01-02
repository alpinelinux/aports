add_library(external_openssl INTERFACE IMPORTED GLOBAL)
add_library(desktop-app::external_openssl ALIAS external_openssl)

find_package(OpenSSL REQUIRED)

target_include_directories(external_openssl SYSTEM INTERFACE ${OpenSSL_INCLUDE_DIR})
target_link_libraries(external_openssl
INTERFACE
	OpenSSL::Crypto
	OpenSSL::SSL
)
