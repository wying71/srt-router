cmake_minimum_required(VERSION 3.20)

project(libwebserver)

include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/common.cmake")

if(NOT TARGET boost)
    add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/../../3rd/boost" 3rd/boost)
endif()

add_definitions(-DNETWORK_URI_EXTERNAL_BOOST)
#add_definitions(-D_WIN32_WINNT)

file(GLOB_RECURSE HEADERS
    include/*.h
    ../libwebsocketcommon/*.h
)

file(GLOB_RECURSE SOURCES
    src/*.cpp
    ../libwebsocketcommon/*.cpp
    ../../3rd/uri/src/*.cpp
)

include_directories(
    ./include
    ../libcommon
    ../
    ../../3rd/websocketpp/include
    ../../3rd/boost/include
    ../../3rd/uri/include
    ../../3rd/openssl/include
)

if(MSVC)
    SET(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}   /bigobj")
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /bigobj")
else()
endif(MSVC)

config_msvc_mt()

add_library(${PROJECT_NAME} STATIC ${HEADERS} ${SOURCES})

set_target_properties(${PROJECT_NAME} PROPERTIES 
    INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_LIST_DIR}/include"
)

target_link_libraries(${PROJECT_NAME}
    boost_chrono
    boost_thread
)

