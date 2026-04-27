cmake_minimum_required(VERSION 3.20)

project(libjson)

include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/common.cmake")

if(WIN32)
else()
  set(CMAKE_SHARED_LIBRARY_PREFIX "")
  set(CMAKE_STATIC_LIBRARY_PREFIX "")
endif(WIN32)

set(LIBJSON_INCLUDE_DIR "${CMAKE_CURRENT_LIST_DIR}/include")

if(MSVC)

else()
  set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}   -fPIC")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")
endif(MSVC)

include_directories(
  ./include)

file(GLOB_RECURSE HEADER_FILES
  include/*.h)

file(GLOB_RECURSE SOURCE_FILES
  src/*.cpp)

config_msvc_mt()

add_library(${PROJECT_NAME} STATIC ${HEADER_FILES} ${SOURCE_FILES})

set_target_properties(${PROJECT_NAME} PROPERTIES
  INTERFACE_INCLUDE_DIRECTORIES "${LIBJSON_INCLUDE_DIR}")

target_link_libraries(${PROJECT_NAME})
