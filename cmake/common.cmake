SET(CMAKE_INCLUDE_CURRENT_DIR       ON)
set(CMAKE_BUILD_WITH_INSTALL_RPATH  TRUE)
set(CMAKE_INSTALL_RPATH             ".")

if(NOT DEFINED FFMPEG_LIB_NAME)
  set(FFMPEG_LIB_NAME "ffmpeg-7.x")
  MESSAGE(STATUS "FFMPEG_LIB_NAME is not defined, default to \"ffmpeg-7.x\"")
endif()

set(CMAKE_CXX_FLAGS_PRODUCT           "${CMAKE_CXX_FLAGS_RELEASE}"            CACHE STRING "C++ Flags"           FORCE)
set(CMAKE_C_FLAGS_PRODUCT             "${CMAKE_C_FLAGS_RELEASE}"              CACHE STRING "C Flags"             FORCE)
set(CMAKE_SHARED_LINKER_FLAGS_PRODUCT "${CMAKE_SHARED_LINKER_FLAGS_RELEASE}"  CACHE STRING "Shared Linker Flags" FORCE)
set(CMAKE_EXE_LINKER_FLAGS_PRODUCT    "${CMAKE_EXE_LINKER_FLAGS_RELEASE}"     CACHE STRING "Exe Linker Flags"    FORCE)

set_property(DIRECTORY APPEND PROPERTY 
  COMPILE_DEFINITIONS 
  $<$<CONFIG:Debug>:_DEBUG;DEBUG> 
  $<$<CONFIG:Release>:NDEBUG> 
  $<$<CONFIG:Product>:NDEBUG;_BUILD_PRODUCT>)

macro(config_zerocreate)
  add_custom_target(ZERO_CREATE ALL)
  set(REMOVE_CMAKE_DIR ${CMAKE_BINARY_DIR}/CMakeFiles)
  if(WIN32)
    string(REPLACE "/" "\\" REMOVE_CMAKE_DIR ${REMOVE_CMAKE_DIR})
    add_custom_command(TARGET ZERO_CREATE POST_BUILD
      COMMAND rmdir /s/q ${REMOVE_CMAKE_DIR})
  else()
    add_custom_command(TARGET ZERO_CREATE POST_BUILD
      COMMAND mv ${CMAKE_BINARY_DIR}/CMakeFiles/Makefile2 ${CMAKE_BINARY_DIR}
      COMMAND rm -rf ${REMOVE_CMAKE_DIR}/*
      COMMAND mv ${CMAKE_BINARY_DIR}/Makefile2 ${CMAKE_BINARY_DIR}/CMakeFiles/Makefile2)
  endif()
endmacro(config_zerocreate)

macro(config_msvc_mt)
  if(MSVC)
    if (CMAKE_VERSION VERSION_LESS 3.15)
      set(CompilerFlags
              CMAKE_CXX_FLAGS
              CMAKE_CXX_FLAGS_DEBUG
              CMAKE_CXX_FLAGS_RELEASE
              CMAKE_CXX_FLAGS_PRODUCT
              CMAKE_C_FLAGS
              CMAKE_C_FLAGS_DEBUG
              CMAKE_C_FLAGS_RELEASE
              CMAKE_C_FLAGS_PRODUCT
          )
      foreach(CompilerFlag ${CompilerFlags})
        string(REPLACE "/MD" "/MT" ${CompilerFlag} "${${CompilerFlag}}")
      endforeach()
    else()
      set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    endif()
  endif()
endmacro(config_msvc_mt)

macro(strip_elf PROJNAME SHARED_LIBRARY)
  if(LINUX)
    if("${SHARED_LIBRARY}" STREQUAL "TRUE")
      set(ONAME "lib${PROJNAME}.so")
    else()
      set(ONAME "${PROJNAME}")
    endif()

    get_target_property(EXEDIR ${PROJNAME} RUNTIME_OUTPUT_DIRECTORY)
    set(_FILENAME "${EXEDIR}/${ONAME}")

    add_custom_command(
      TARGET ${PROJNAME}
      POST_BUILD
      COMMAND "objcopy" "--only-keep-debug" "${_FILENAME}" "${_FILENAME}.debug")

    add_custom_command(
      TARGET ${PROJNAME}
      POST_BUILD
      COMMAND "strip" "-s" "${_FILENAME}")

    add_custom_command(
      TARGET ${PROJNAME}
      POST_BUILD
      COMMAND "objcopy" "--add-gnu-debuglink=${_FILENAME}.debug" "${_FILENAME}")
  endif()
endmacro(strip_elf)

macro(config_exclude_libs)
  if(WIN32)
  else()
    SET(CMAKE_SHARED_LINKER_FLAGS   "${CMAKE_SHARED_LINKER_FLAGS}   -Wl,-Bsymbolic -Wl,--exclude-libs,ALL")
    SET(CMAKE_MODULE_LINKER_FLAGS   "${CMAKE_MODULE_LINKER_FLAGS}   -Wl,-Bsymbolic -Wl,--exclude-libs,ALL")
  endif()
endmacro(config_exclude_libs)
