cmake_minimum_required(VERSION 3.10)

set(CMAKE_MACOSX_RPATH 1)

IF (APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.9" CACHE STRING "Minimum OS X deployment version" FORCE)
ENDIF(APPLE)

project(_aicspylibczi)
# build problems with library dependencies if I go above C++14
# namely the dependencies on the Eigen library and they have to update
# their use of std::bind2nd / std::bind1st to std::bind
#set(CMAKE_CXX_COMPILER /Applications/CLion.app/Contents/bin/clang)


set(CMAKE_CXX_STANDARD 14)

IF (WIN32)
    set(PYBIND11_CPP_STANDARD /std:c++14)
ELSE()
    # linking with 'thread' is necessary if we use std::thread and related under Linux it seems
    #   otherwise - the program simply crashes (no build-error)
    #set(CMAKE_C_COMPILER clang)
    #set(CMAKE_CXX_COMPILER clang++)
    set(PYBIND11_CPP_STANDARD -std=c++14)
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread -std=c++14 -fPIC -O0 -g -D_FILE_OFFSET_BITS=64 -fvisibility=hidden")
    SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D__ANSI__ -fPIC -D_FILE_OFFSET_BITS=64")
    add_compile_definitions(LINUXENV)

ENDIF(WIN32)

SET(pybind11_DIR ${CMAKE_CURRENT_LIST_DIR}/pybind11)

add_compile_definitions(_LIBCZISTATICLIB)

# Turn off the parts of libCZI we do not link against. These must be cache variables because
# libCZI declares them with option(); as add_compile_definitions() they only became -D
# preprocessor flags and left the options at their defaults, so every build was fetching
# googletest from GitHub and compiling libCZI's whole test suite and its dynamic library.
# The googletest download made configure fail whenever GitHub returned a 503.
set(LIBCZI_BUILD_UNITTESTS OFF CACHE BOOL "" FORCE)
set(LIBCZI_BUILD_DYNLIB OFF CACHE BOOL "" FORCE)
set(LIBCZI_BUILD_CZICMD OFF CACHE BOOL "" FORCE)

# Build libCZI's curl-based http/https stream class so CZI files can be read from URLs.
# These must be cache variables because libCZI declares them with option().
set(LIBCZI_BUILD_CURL_BASED_STREAM ON CACHE BOOL "" FORCE)
# OFF makes libCZI fetch and statically link a pinned libcurl, keeping wheels self-contained.
set(LIBCZI_BUILD_PREFER_EXTERNALPACKAGE_LIBCURL OFF CACHE BOOL "" FORCE)

IF (APPLE)
    # Use Apple's Secure Transport for TLS rather than OpenSSL. curl's CMake picks up a
    # Homebrew OpenSSL when it can find one, which is not redistributable in a wheel and is
    # single-architecture, so it breaks the universal2 build. Secure Transport needs no
    # external dependency and uses the system trust store.
    set(CURL_USE_SECTRANSP ON CACHE BOOL "" FORCE)
    set(CURL_USE_OPENSSL OFF CACHE BOOL "" FORCE)
ENDIF(APPLE)

# Pin off curl's optional dependencies. They are auto-detected, so without this the wheel
# picks up whatever happens to be installed on the build machine - libidn2 from Homebrew,
# for example, which is not redistributable. None are needed to issue range-request GETs.
set(USE_LIBIDN2 OFF CACHE BOOL "" FORCE)
set(CURL_USE_LIBPSL OFF CACHE BOOL "" FORCE)
set(CURL_USE_LIBSSH2 OFF CACHE BOOL "" FORCE)
set(USE_NGHTTP2 OFF CACHE BOOL "" FORCE)
set(CURL_BROTLI OFF CACHE BOOL "" FORCE)
set(CURL_ZSTD OFF CACHE BOOL "" FORCE)
set(CURL_DISABLE_LDAP ON CACHE BOOL "" FORCE)
set(CURL_DISABLE_LDAPS ON CACHE BOOL "" FORCE)

add_subdirectory(libCZI)
add_subdirectory(pybind11)

message(STATUS "Found pybind11 v${pybind11_VERSION}: ${pybind11_INCLUDE_DIRS}")

set(PYLIBCZI_C_SRC_HEADERS _aicspylibczi/Reader.h _aicspylibczi/helper_algorithms.h _aicspylibczi/exceptions.h _aicspylibczi/inc_libCZI.h
        _aicspylibczi/IndexMap.h _aicspylibczi/Image.h _aicspylibczi/TypedImage.h _aicspylibczi/ImageFactory.h
        _aicspylibczi/SourceRange.h _aicspylibczi/TargetRange.h _aicspylibczi/pylibczi_ostream.h
        _aicspylibczi/SubblockMetaVec.h _aicspylibczi/DimIndex.h _aicspylibczi/constants.h
        _aicspylibczi/StreamImplLockingRead.h _aicspylibczi/Threadpool.h _aicspylibczi/UrlStream.h)

set(PYLIBCZI_C_SRC _aicspylibczi/Reader.cpp _aicspylibczi/IndexMap.cpp _aicspylibczi/Image.cpp _aicspylibczi/ImageFactory.cpp
        _aicspylibczi/pylibczi_ostream.cpp _aicspylibczi/exceptions.cpp _aicspylibczi/SubblockSortable.h
        _aicspylibczi/pb_caster_SubblockMetaVec.h _aicspylibczi/constants.cpp _aicspylibczi/DimIndex.cpp
        _aicspylibczi/StreamImplLockingRead.cpp _aicspylibczi/UrlStream.cpp)

set(PYLIBCZI_PYBIND11 _aicspylibczi/pb_bindings.cpp _aicspylibczi/pb_helpers.h _aicspylibczi/pb_helpers.cpp _aicspylibczi/pb_caster_ImagesContainer.h
        _aicspylibczi/pb_caster_BytesIO.h _aicspylibczi/pb_caster_libCZI_DimensionIndex.h _aicspylibczi/CSimpleStreamImplFromFd.h
        _aicspylibczi/CSimpleStreamImplFromFd.cpp _aicspylibczi/pb_caster_DimIndex.h
        _aicspylibczi/pb_stream_options.h)

set(TARGET_ONE libczi_c++_extension)
set(TARGET_TWO _aicspylibczi)
set(TARGET_THREE profile_me)

add_library(${TARGET_ONE} STATIC ${PYLIBCZI_C_SRC} ${PYLIBCZI_C_SRC_HEADERS})
target_include_directories(${TARGET_ONE} PUBLIC ${CMAKE_SOURCE_DIR}/libCZI/Src)
target_link_libraries(${TARGET_ONE} INTERFACE libCZIStatic JxrDecodeStatic)
add_dependencies(${TARGET_ONE} libCZIStatic JxrDecodeStatic)

pybind11_add_module(${TARGET_TWO} MODULE ${PYLIBCZI_C_SRC} ${PYLIBCZI_PYBIND11})
target_include_directories(${TARGET_TWO} PUBLIC ${CMAKE_SOURCE_DIR}/libCZI/Src)
target_link_libraries(${TARGET_TWO} PRIVATE libCZIStatic JxrDecodeStatic)
add_dependencies(${TARGET_TWO} libCZIStatic JxrDecodeStatic)
add_custom_command(TARGET ${TARGET_TWO} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${TARGET_TWO}> ${PROJECT_SOURCE_DIR}
        )

add_subdirectory(c_tests)

add_executable(${TARGET_THREE} main.cpp)
target_link_libraries(${TARGET_THREE} ${TARGET_ONE})

if(DEFINED ENV{BUILD_DOCS})
    add_subdirectory(docs)
endif()
