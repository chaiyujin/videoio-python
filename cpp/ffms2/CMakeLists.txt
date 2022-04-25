cmake_minimum_required(VERSION 3.14)
project(ffms2)
set(CMAKE_C_STANDARD            11)
set(CMAKE_CXX_STANDARD          17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# on windows
if (WIN32)
    list(APPEND CMAKE_PREFIX_PATH  "C:\\msys64\\mingw64")
endif (WIN32)

# zlib
find_package(ZLIB REQUIRED)
list(APPEND _ffms2_libs ZLIB::ZLIB)
# find_package(ZLIB)
# if (ZLIB_FOUND)
#     list(APPEND _ffms2_libs ZLIB::ZLIB)
# else()
#     # message(SEND_ERROR "Failed to find zlib")
#     add_subdirectory(../zlib-1.2.11 zlib)
#     list(APPEND _ffms2_libs zlib)
#     list(APPEND _ffms2_include_dirs ../zlib-1.2.11)  # zlib.h
#     list(APPEND _ffms2_include_dirs ${CMAKE_CURRENT_BINARY_DIR}/zlib)  # zconf.h
# endif(ZLIB_FOUND)

# ffmpeg
find_package(FFmpeg  REQUIRED)
# threads
find_package(Threads REQUIRED)
# libs
list(APPEND _ffms2_libs FFmpeg::FFmpeg Threads::Threads)

# include dirs
list(APPEND _ffms2_include_dirs
            ${PROJECT_SOURCE_DIR}/include
            ${PROJECT_SOURCE_DIR}/src/core)

# fetch codes
file(GLOB_RECURSE _ffms2_headers ${PROJECT_SOURCE_DIR}/*.h   ${PROJECT_SOURCE_DIR}/*.hpp)
file(GLOB_RECURSE _ffms2_sources ${PROJECT_SOURCE_DIR}/*.cxx ${PROJECT_SOURCE_DIR}/*.cpp)

# add library
# add_definitions("-DFFMS_WITH_DEPRECATED")  # enbale DEPRECATED
set(_ffms2_definitions "-DFFMS_STATIC")
if (WIN32 AND NOT SNOW_IS_RELEASE)
    list(APPEND _ffms2_definitions "-DFFMS_WIN_DEBUG")
endif ()
add_library               (${PROJECT_NAME} STATIC ${_ffms2_headers} ${_ffms2_sources} ${BACKWARD_ENABLE})
target_include_directories(${PROJECT_NAME} PUBLIC ${_ffms2_include_dirs})
target_link_libraries     (${PROJECT_NAME} PUBLIC ${_ffms2_libs})
target_compile_definitions(${PROJECT_NAME} PUBLIC ${_ffms2_definitions})
