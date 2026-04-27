cmake_minimum_required(VERSION 3.20)

if(WIN32)
  project(SrtRouter)
else()
  project(srtrouter)
endif()

include(../cmake/common.cmake)

if(NOT DEFINED DESTINATION_DIR OR DESTINATION_DIR STREQUAL "")
  set(DESTINATION_DIR "${CMAKE_CURRENT_LIST_DIR}/build/bin")
endif()

config_msvc_mt()

set(CMAKE_CONFIGURATION_TYPES "Debug;Release")

if(NOT TARGET boost)
  add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../3rd/boost            3rd/boost)
endif()
if(NOT TARGET libjson)
  add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../3rd/libjson          3rd/libjson)
endif()
#if(NOT TARGET libcurl)
#  add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../3rd/libcurl          3rd/libcurl)
#endif()
if(NOT TARGET openssl)
  add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../3rd/openssl          3rd/openssl)
endif()
#if(NOT TARGET zlib)
#  add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../3rd/zlib             3rd/zlib)
#endif()
if(NOT TARGET spdlog)
  add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../3rd/spdlog           3rd/spdlog)
endif()

if(NOT TARGET libcommons)
  add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../lib/libcommon        lib/libcommon)
endif()
if(NOT TARGET libwebserver)
  add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../lib/libwebserver     lib/libwebserver)
endif()

include_directories(
  ./
  ${CMAKE_CURRENT_LIST_DIR}/../lib/libcommon
  ${COMMMON_LIBS_3RD}
  ${COMMMON_LIBS_3RD}/boost
  ${COMMMON_LIBS_3RD}/libjson/include
  ${COMMMON_LIBS_3RD}/spdlog/include
  )

if(MSVC)
else()
  set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}   -Wno-deprecated-declarations -fPIC -O3 -fvisibility=hidden -s -std=gnu99 ")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-deprecated-declarations -fPIC -O3 -fvisibility=hidden -s -std=c++17 -fpermissive")
endif(MSVC)

file(GLOB_RECURSE ENCODER_HEADERS 
  *.h
  *.rc)
file(GLOB ENCODER_SOURCES
  *.cpp)

add_executable(${PROJECT_NAME} ${ENCODER_HEADERS} ${ENCODER_SOURCES})

if(LINUX AND CMAKE_PRODUCT_BUILD_TYPE STREQUAL "release")
  strip_elf(${PROJECT_NAME} "FALSE")
  config_exclude_libs()
endif()

if(WIN32)
  set_property(TARGET ${PROJECT_NAME} PROPERTY LINK_FLAGS "/ignore:4099")
endif()

set(LIBS
  boost
  boost_filesystem
  libcommons
  libwebserver
#  libcurl
  libjson
  spdlog
  openssl
#  zlib
  )

target_link_libraries(${PROJECT_NAME} ${LIBS})

install(FILES ./version.properties DESTINATION ${DESTINATION_DIR} COMPONENT application)
install(FILES ./srtrouter.readme.md DESTINATION ${DESTINATION_DIR} COMPONENT application)
install(FILES ./srtrouterconfig.json DESTINATION ${DESTINATION_DIR} COMPONENT application)
install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION ${DESTINATION_DIR} COMPONENT application)
