cmake_minimum_required(VERSION 3.17)
project(ffutils)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)

# include cmake utils
list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")
include(cmake/Fetch.cmake)

# NOTE: Please set this by -DFFmpeg_INSTALL_PATH
if (NOT FFmpeg_INSTALL_PATH)
    # ffmpeg from brew
    if (APPLE)
        set(FFmpeg_INSTALL_PATH "/usr/local/Cellar/ffmpeg/5.1.1")
    endif (APPLE)
endif()
find_package(FFmpeg REQUIRED)
find_package(Threads REQUIRED)
list(APPEND link_libraries FFmpeg::FFmpeg Threads::Threads)

# spdlog
GitHelper(spdlog https://github.com/gabime/spdlog.git v1.10.0 TRUE "" "spdlog")

# pybind 11
find_package(pybind11)
if (NOT pybind11_FOUND)
    GitHelper(pybind11 https://github.com/pybind/pybind11.git v2.9.2 TRUE "" "")
endif ()

list(APPEND sources
    common.cpp
    stream.cpp
    video_reader.cpp
    video_writer.cpp
)

pybind11_add_module       (videoio ${headers} ${sources} pybind.cpp)
target_include_directories(videoio PRIVATE ${include_directories})
target_link_directories   (videoio PRIVATE ${link_directories})
target_link_libraries     (videoio PRIVATE ${link_libraries})
target_compile_definitions(videoio PRIVATE ${compile_definitions} ${definitions})
target_compile_options    (videoio PRIVATE -Wall -Wextra -Wpedantic -Werror)

add_executable(main test.cpp ${sources})
target_include_directories(main PUBLIC ${include_directories})
target_link_directories   (main PUBLIC ${link_directories})
target_link_libraries     (main PUBLIC ${link_libraries})
target_compile_definitions(main PUBLIC ${compile_definitions} ${definitions})
# target_compile_options(main PRIVATE -Wall -Wextra -Wpedantic -Werror)
