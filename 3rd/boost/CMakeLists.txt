cmake_minimum_required(VERSION 3.20)

project(boost)

include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/common.cmake")

add_library(boost             SHARED IMPORTED GLOBAL)
add_library(boost_atomic      SHARED IMPORTED GLOBAL)
add_library(boost_chrono      SHARED IMPORTED GLOBAL)
add_library(boost_date_time   SHARED IMPORTED GLOBAL)
add_library(boost_filesystem  SHARED IMPORTED GLOBAL)
add_library(boost_random      SHARED IMPORTED GLOBAL)
add_library(boost_regex       SHARED IMPORTED GLOBAL)
add_library(boost_system      SHARED IMPORTED GLOBAL)
add_library(boost_thread      SHARED IMPORTED GLOBAL)

set(BOOST_LIB_DIR     "${CMAKE_CURRENT_LIST_DIR}/lib")
set(BOOST_INCLUDE_DIR "${CMAKE_CURRENT_LIST_DIR}/..")

if(WIN32)
  set_target_properties(boost PROPERTIES
    IMPORTED_IMPLIB_DEBUG   "${BOOST_LIB_DIR}/win/libboost_date_time-vc142-mt-sgd-x64-1_80.lib"
    IMPORTED_IMPLIB_RELEASE "${BOOST_LIB_DIR}/win/libboost_date_time-vc142-mt-s-x64-1_80.lib"
    IMPORTED_IMPLIB_PRODUCT "${BOOST_LIB_DIR}/win/libboost_date_time-vc142-mt-s-x64-1_80.lib"
    INTERFACE_INCLUDE_DIRECTORIES "${BOOST_INCLUDE_DIR}")

  set_target_properties(boost_atomic PROPERTIES
    IMPORTED_IMPLIB_DEBUG   "${BOOST_LIB_DIR}/win/libboost_atomic-vc142-mt-sgd-x64-1_80.lib"
    IMPORTED_IMPLIB_RELEASE "${BOOST_LIB_DIR}/win/libboost_atomic-vc142-mt-s-x64-1_80.lib"
    IMPORTED_IMPLIB_PRODUCT "${BOOST_LIB_DIR}/win/libboost_atomic-vc142-mt-s-x64-1_80.lib"
    INTERFACE_INCLUDE_DIRECTORIES "${BOOST_INCLUDE_DIR}")

  set_target_properties(boost_chrono PROPERTIES
    IMPORTED_IMPLIB_DEBUG   "${BOOST_LIB_DIR}/win/libboost_chrono-vc142-mt-sgd-x64-1_80.lib"
    IMPORTED_IMPLIB_RELEASE "${BOOST_LIB_DIR}/win/libboost_chrono-vc142-mt-s-x64-1_80.lib"
    IMPORTED_IMPLIB_PRODUCT "${BOOST_LIB_DIR}/win/libboost_chrono-vc142-mt-s-x64-1_80.lib"
    INTERFACE_INCLUDE_DIRECTORIES "${BOOST_INCLUDE_DIR}")

  set_target_properties(boost_date_time PROPERTIES
    IMPORTED_IMPLIB_DEBUG   "${BOOST_LIB_DIR}/win/libboost_date_time-vc142-mt-sgd-x64-1_80.lib"
    IMPORTED_IMPLIB_RELEASE "${BOOST_LIB_DIR}/win/libboost_date_time-vc142-mt-s-x64-1_80.lib"
    IMPORTED_IMPLIB_PRODUCT "${BOOST_LIB_DIR}/win/libboost_date_time-vc142-mt-s-x64-1_80.lib"
    INTERFACE_INCLUDE_DIRECTORIES "${BOOST_INCLUDE_DIR}")

  set_target_properties(boost_filesystem PROPERTIES
    IMPORTED_IMPLIB_DEBUG   "${BOOST_LIB_DIR}/win/libboost_filesystem-vc142-mt-sgd-x64-1_80.lib"
    IMPORTED_IMPLIB_RELEASE "${BOOST_LIB_DIR}/win/libboost_filesystem-vc142-mt-s-x64-1_80.lib"
    IMPORTED_IMPLIB_PRODUCT "${BOOST_LIB_DIR}/win/libboost_filesystem-vc142-mt-s-x64-1_80.lib"
    INTERFACE_INCLUDE_DIRECTORIES "${BOOST_INCLUDE_DIR}")

  set_target_properties(boost_random PROPERTIES
    IMPORTED_IMPLIB_DEBUG   "${BOOST_LIB_DIR}/win/libboost_random-vc142-mt-sgd-x64-1_80.lib"
    IMPORTED_IMPLIB_RELEASE "${BOOST_LIB_DIR}/win/libboost_random-vc142-mt-s-x64-1_80.lib"
    IMPORTED_IMPLIB_PRODUCT "${BOOST_LIB_DIR}/win/libboost_random-vc142-mt-s-x64-1_80.lib"
    INTERFACE_INCLUDE_DIRECTORIES "${BOOST_INCLUDE_DIR}")

  set_target_properties(boost_regex PROPERTIES
    IMPORTED_IMPLIB_DEBUG   "${BOOST_LIB_DIR}/win/libboost_regex-vc142-mt-sgd-x64-1_80.lib"
    IMPORTED_IMPLIB_RELEASE "${BOOST_LIB_DIR}/win/libboost_regex-vc142-mt-s-x64-1_80.lib"
    IMPORTED_IMPLIB_PRODUCT "${BOOST_LIB_DIR}/win/libboost_regex-vc142-mt-s-x64-1_80.lib"
    INTERFACE_INCLUDE_DIRECTORIES "${BOOST_INCLUDE_DIR}")

  set_target_properties(boost_system PROPERTIES
    IMPORTED_IMPLIB_DEBUG   "${BOOST_LIB_DIR}/win/libboost_system-vc142-mt-sgd-x64-1_80.lib"
    IMPORTED_IMPLIB_RELEASE "${BOOST_LIB_DIR}/win/libboost_system-vc142-mt-s-x64-1_80.lib"
    IMPORTED_IMPLIB_PRODUCT "${BOOST_LIB_DIR}/win/libboost_system-vc142-mt-s-x64-1_80.lib"
    INTERFACE_INCLUDE_DIRECTORIES "${BOOST_INCLUDE_DIR}")

  set_target_properties(boost_thread PROPERTIES
    IMPORTED_IMPLIB_DEBUG   "${BOOST_LIB_DIR}/win/libboost_thread-vc142-mt-sgd-x64-1_80.lib"
    IMPORTED_IMPLIB_RELEASE "${BOOST_LIB_DIR}/win/libboost_thread-vc142-mt-s-x64-1_80.lib"
    IMPORTED_IMPLIB_PRODUCT "${BOOST_LIB_DIR}/win/libboost_thread-vc142-mt-s-x64-1_80.lib"
    INTERFACE_INCLUDE_DIRECTORIES "${BOOST_INCLUDE_DIR}")
else()
  set_target_properties(boost PROPERTIES
    IMPORTED_LOCATION_DEBUG   "${BOOST_LIB_DIR}/linux/libboost_date_time.a"
    IMPORTED_LOCATION_RELEASE "${BOOST_LIB_DIR}/linux/libboost_date_time.a"
    IMPORTED_LOCATION_PRODUCT "${BOOST_LIB_DIR}/linux/libboost_date_time.a"
    INTERFACE_INCLUDE_DIRECTORIES "${BOOST_INCLUDE_DIR}")

  set_target_properties(boost_atomic PROPERTIES
    IMPORTED_LOCATION_DEBUG   "${BOOST_LIB_DIR}/linux/libboost_atomic.a"
    IMPORTED_LOCATION_RELEASE "${BOOST_LIB_DIR}/linux/libboost_atomic.a"
    IMPORTED_LOCATION_PRODUCT "${BOOST_LIB_DIR}/linux/libboost_atomic.a"
    INTERFACE_INCLUDE_DIRECTORIES "${BOOST_INCLUDE_DIR}")

  set_target_properties(boost_chrono PROPERTIES
    IMPORTED_LOCATION_DEBUG   "${BOOST_LIB_DIR}/linux/libboost_chrono.a"
    IMPORTED_LOCATION_RELEASE "${BOOST_LIB_DIR}/linux/libboost_chrono.a"
    IMPORTED_LOCATION_PRODUCT "${BOOST_LIB_DIR}/linux/libboost_chrono.a"
    INTERFACE_INCLUDE_DIRECTORIES "${BOOST_INCLUDE_DIR}")

  set_target_properties(boost_date_time PROPERTIES
    IMPORTED_LOCATION_DEBUG   "${BOOST_LIB_DIR}/linux/libboost_date_time.a"
    IMPORTED_LOCATION_RELEASE "${BOOST_LIB_DIR}/linux/libboost_date_time.a"
    IMPORTED_LOCATION_PRODUCT "${BOOST_LIB_DIR}/linux/libboost_date_time.a"
    INTERFACE_INCLUDE_DIRECTORIES "${BOOST_INCLUDE_DIR}")

  set_target_properties(boost_filesystem PROPERTIES
    IMPORTED_LOCATION_DEBUG   "${BOOST_LIB_DIR}/linux/libboost_filesystem.a"
    IMPORTED_LOCATION_RELEASE "${BOOST_LIB_DIR}/linux/libboost_filesystem.a"
    IMPORTED_LOCATION_PRODUCT "${BOOST_LIB_DIR}/linux/libboost_filesystem.a"
    INTERFACE_INCLUDE_DIRECTORIES "${BOOST_INCLUDE_DIR}")

  set_target_properties(boost_random PROPERTIES
    IMPORTED_LOCATION_DEBUG   "${BOOST_LIB_DIR}/linux/libboost_random.a"
    IMPORTED_LOCATION_RELEASE "${BOOST_LIB_DIR}/linux/libboost_random.a"
    IMPORTED_LOCATION_PRODUCT "${BOOST_LIB_DIR}/linux/libboost_random.a"
    INTERFACE_INCLUDE_DIRECTORIES "${BOOST_INCLUDE_DIR}")

  set_target_properties(boost_regex PROPERTIES
    IMPORTED_LOCATION_DEBUG   "${BOOST_LIB_DIR}/linux/libboost_regex.a"
    IMPORTED_LOCATION_RELEASE "${BOOST_LIB_DIR}/linux/libboost_regex.a"
    IMPORTED_LOCATION_PRODUCT "${BOOST_LIB_DIR}/linux/libboost_regex.a"
    INTERFACE_INCLUDE_DIRECTORIES "${BOOST_INCLUDE_DIR}")

  set_target_properties(boost_system PROPERTIES
    IMPORTED_LOCATION_DEBUG   "${BOOST_LIB_DIR}/linux/libboost_system.a"
    IMPORTED_LOCATION_RELEASE "${BOOST_LIB_DIR}/linux/libboost_system.a"
    IMPORTED_LOCATION_PRODUCT "${BOOST_LIB_DIR}/linux/libboost_system.a"
    INTERFACE_INCLUDE_DIRECTORIES "${BOOST_INCLUDE_DIR}")

  set_target_properties(boost_thread PROPERTIES
    IMPORTED_LOCATION_DEBUG   "${BOOST_LIB_DIR}/linux/libboost_thread.a"
    IMPORTED_LOCATION_RELEASE "${BOOST_LIB_DIR}/linux/libboost_thread.a"
    IMPORTED_LOCATION_PRODUCT "${BOOST_LIB_DIR}/linux/libboost_thread.a"
    INTERFACE_INCLUDE_DIRECTORIES "${BOOST_INCLUDE_DIR}")
endif()
