cmake_minimum_required(VERSION 3.17)
project(ffutils)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
SET(CMAKE_POSITION_INDEPENDENT_CODE ON)

# set(CMAKE_CXX_FLAGS "-O3 -Wall -Wextra")
# set(CMAKE_CXX_FLAGS_DEBUG "-g")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

# include cmake utils
list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")
include(cmake/Fetch.cmake)  # GitHelper

GitHelper(spdlog https://github.com/gabime/spdlog.git v1.10.0 FALSE "include" "")

# ffmpeg from brew
if (APPLE)
    set(FFmpeg_INSTALL_PATH "/usr/local/Cellar/ffmpeg/5.0.1")
endif (APPLE)
find_package(FFmpeg REQUIRED)
find_package(Threads REQUIRED)
list(APPEND link_libraries FFmpeg::FFmpeg Threads::Threads)

add_executable(read_video src/main.cpp src/ffutils/common.cpp src/ffutils/video_reader.cpp)
target_include_directories(read_video PUBLIC ${include_directories})
target_link_directories   (read_video PUBLIC ${link_directories})
target_link_libraries     (read_video PUBLIC ${link_libraries})
target_compile_definitions(read_video PUBLIC ${compile_definitions} ${definitions})

# nanobind and Python
GitHelper(nanobind https://github.com/wjakob/nanobind.git e4af8366dd74481e009ebb3e28ba13ab64d3fb50 TRUE "" "")
set(Python_FIND_FRAMEWORK LAST) # Prefer Brew/Conda to Apple framework python
find_package(Python 3.8 COMPONENTS Interpreter Development Development.Module REQUIRED)

# add sources
nanobind_add_module       (ffutils ${headers} ${sources} src/bind.cpp src/ffutils/common.cpp src/ffutils/video_reader.cpp)
target_include_directories(ffutils PRIVATE ${include_directories})
target_link_directories   (ffutils PRIVATE ${link_directories})
target_link_libraries     (ffutils PRIVATE ${link_libraries})
target_compile_definitions(ffutils PRIVATE ${compile_definitions} ${definitions})
