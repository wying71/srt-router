cmake_minimum_required(VERSION 3.20)

project(libcommons)

include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/common.cmake")

set(LIBCOM_INCLUDE_DIR "${CMAKE_CURRENT_LIST_DIR}")

if(WIN32)

else()
  set(CMAKE_SHARED_LIBRARY_PREFIX "")
  set(CMAKE_STATIC_LIBRARY_PREFIX "")
endif(WIN32)

set(CMAKE_CONFIGURATION_TYPES "Debug;Release")

# if(NOT TARGET ${FFMPEG_LIB_NAME})
#   add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/../../3rd/${FFMPEG_LIB_NAME}"     "3rd/${FFMPEG_LIB_NAME}")
# endif()

if(NOT TARGET srt)
  add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/../../3rd/srt-1.5.4"              "3rd/srt-1.5.4")
endif()

if(NOT TARGET libjson)
  add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/../../3rd/libjson"                "3rd/libjson")
endif()

add_definitions(-D_CRT_SECURE_NO_WARNINGS)

if(MSVC)
  set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}   /O2 /Ob2")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /O2 /Ob2")
else()
  set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}   -fPIC -O3 -fvisibility=hidden -s -std=gnu99 ")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC -O3 -fvisibility=hidden -s -std=c++17 -fpermissive")
endif(MSVC)

include_directories(
  ./
  ../
)

file(GLOB HEADERS
*.h
)

file(GLOB SOURCES 
./*.cpp
)

config_msvc_mt()

add_library(${PROJECT_NAME} STATIC ${HEADERS} ${SOURCES})

set_target_properties(${PROJECT_NAME} PROPERTIES
  INTERFACE_INCLUDE_DIRECTORIES "${LIBCOM_INCLUDE_DIR}")

target_link_libraries(${PROJECT_NAME} 
# ${FFMPEG_LIB_NAME}
srt
libjson)
