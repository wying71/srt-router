cmake_minimum_required(VERSION 3.20)

project(srt)

include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/common.cmake")

add_library(${PROJECT_NAME} SHARED IMPORTED GLOBAL)

set(SRT_BIN_DIR     "${CMAKE_CURRENT_LIST_DIR}/bin")
set(SRT_LIB_DIR     "${CMAKE_CURRENT_LIST_DIR}/lib")
set(SRT_INCLUDE_DIR "${CMAKE_CURRENT_LIST_DIR}/include")

if(WIN32)
  set(SRT_LIBRARY   "${SRT_LIB_DIR}/win/srt.lib")
else()
  set(SRT_LIBRARY   "${SRT_LIB_DIR}/linux/libsrt.so")
endif()

if(WIN32)
  set_target_properties(${PROJECT_NAME} PROPERTIES
    IMPORTED_IMPLIB_DEBUG     "${SRT_LIBRARY}"
    IMPORTED_IMPLIB_RELEASE   "${SRT_LIBRARY}"
    IMPORTED_IMPLIB_PRODUCT   "${SRT_LIBRARY}")
else()
  set_target_properties(${PROJECT_NAME} PROPERTIES 
    IMPORTED_LOCATION_DEBUG   "${SRT_LIBRARY}"
    IMPORTED_LOCATION_RELEASE "${SRT_LIBRARY}"
  IMPORTED_LOCATION_PRODUCT   "${SRT_LIBRARY}")
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES
  INTERFACE_INCLUDE_DIRECTORIES "${SRT_INCLUDE_DIR}")

if(WIN32)
  install(FILES
    "${SRT_BIN_DIR}/win/srt.dll"
    DESTINATION "${DESTINATION_DIR}"
    COMPONENT application)
else()
  install(FILES
    "${SRT_LIB_DIR}/linux/libsrt.so"
    "${SRT_LIB_DIR}/linux/libsrt.so.1.5"
    "${SRT_LIB_DIR}/linux/libsrt.so.1.5.4"
    DESTINATION "${DESTINATION_DIR}"
    COMPONENT application)
endif()
