cmake_minimum_required(VERSION 3.17)
project(ffms_python)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "-O3 -Wall -Wextra")
# set(CMAKE_CXX_FLAGS_DEBUG "-g")
# set(CMAKE_CXX_FLAGS_RELEASE "-O3")

# include cmake utils
list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")
include(cmake/Fetch.cmake)  # GitHelper

GitHelper(spdlog https://github.com/gabime/spdlog.git v1.10.0 FALSE "include" "")

set(FFmpeg_INSTALL_PATH "/usr/local/Cellar/ffmpeg/5.0.1")
find_package(FFmpeg REQUIRED)
find_package(Threads REQUIRED)
add_executable(read_video src/read_video.cpp src/ffmpeg/ffmpeg_fn.cpp)
target_include_directories(read_video PUBLIC ${include_directories})
target_link_directories   (read_video PUBLIC ${link_directories})
target_link_libraries     (read_video PUBLIC ${link_libraries} FFmpeg::FFmpeg Threads::Threads)
target_compile_definitions(read_video PUBLIC ${compile_definitions} ${definitions})

# # ffms2
# add_subdirectory(ffms2)
# list(APPEND link_libraries ffms2)

# # nanobind and Python
# GitHelper(nanobind https://github.com/wjakob/nanobind.git master TRUE "" "")
# set(Python_FIND_FRAMEWORK LAST) # Prefer Brew/Conda to Apple framework python
# find_package(Python 3.8 COMPONENTS Interpreter Development Development.Module REQUIRED)

# # add sources
# list(APPEND sources src/video_reader.cpp)

# nanobind_add_module(ffms ${headers} ${sources} src/bind.cpp)
# target_include_directories(ffms PRIVATE ${include_directories})
# target_link_directories   (ffms PRIVATE ${link_directories})
# target_link_libraries     (ffms PRIVATE ${link_libraries})
# target_compile_definitions(ffms PRIVATE ${compile_definitions} ${definitions})

# # # build cpp test
# # add_executable            (test_cpp ${headers} ${sources} src/main.cpp)
# # target_include_directories(test_cpp PUBLIC ${include_directories})
# # target_link_directories   (test_cpp PUBLIC ${link_directories})
# # target_link_libraries     (test_cpp PUBLIC ${link_libraries})
# # target_compile_definitions(test_cpp PUBLIC ${compile_definitions} ${definitions})
