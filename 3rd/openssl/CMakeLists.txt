cmake_minimum_required(VERSION 3.20)

project(openssl)

include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/common.cmake")

add_library(${PROJECT_NAME} SHARED IMPORTED GLOBAL)

set(OPENSSL_BIN_DIR     "${CMAKE_CURRENT_LIST_DIR}/bin")
set(OPENSSL_LIB_DIR     "${CMAKE_CURRENT_LIST_DIR}/lib")
set(OPENSSL_INCLUDE_DIR "${CMAKE_CURRENT_LIST_DIR}/include")

if(WIN32)
  set(LIBCRYPTO_LIBRARY "${OPENSSL_LIB_DIR}/win/libcrypto.lib")
  set(LIBSSL_LIBRARY    "${OPENSSL_LIB_DIR}/win/libssl.lib")
else()
  set(LIBCRYPTO_LIBRARY "${OPENSSL_LIB_DIR}/linux/libcrypto.so")
  set(LIBSSL_LIBRARY    "${OPENSSL_LIB_DIR}/linux/libssl.so")
endif()

if(WIN32)
  set_target_properties(${PROJECT_NAME} PROPERTIES
    IMPORTED_IMPLIB_DEBUG     "${LIBSSL_LIBRARY}"
    IMPORTED_IMPLIB_RELEASE   "${LIBSSL_LIBRARY}"
    IMPORTED_IMPLIB_PRODUCT   "${LIBSSL_LIBRARY}")
else()
  set_target_properties(${PROJECT_NAME} PROPERTIES
    IMPORTED_LOCATION_DEBUG   "${LIBSSL_LIBRARY}"
    IMPORTED_LOCATION_RELEASE "${LIBSSL_LIBRARY}"
    IMPORTED_LOCATION_PRODUCT "${LIBSSL_LIBRARY}")
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES
  INTERFACE_INCLUDE_DIRECTORIES "${OPENSSL_INCLUDE_DIR}"
  INTERFACE_LINK_LIBRARIES "${LIBCRYPTO_LIBRARY}")

if(WIN32)
  install(FILES 
    "${OPENSSL_BIN_DIR}/win/libcrypto-1_1-x64.dll"
    "${OPENSSL_BIN_DIR}/win/libssl-1_1-x64.dll"
    DESTINATION "${DESTINATION_DIR}"
    COMPONENT application)
else()
  install(FILES 
    "${OPENSSL_LIB_DIR}/linux/libcrypto.so"
    "${OPENSSL_LIB_DIR}/linux/libcrypto.so.1.1"
    "${OPENSSL_LIB_DIR}/linux/libssl.so"
    "${OPENSSL_LIB_DIR}/linux/libssl.so.1.1"
    DESTINATION "${DESTINATION_DIR}"
    COMPONENT application)
endif()
