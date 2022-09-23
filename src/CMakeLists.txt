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

# ffmpeg from brew
if (APPLE)
    set(FFmpeg_INSTALL_PATH "/usr/local/Cellar/ffmpeg/5.1.1")
    # set(FFmpeg_INSTALL_PATH "/usr/local/Cellar/ffmpeg@4/4.4.2_3")
    # set(FFmpeg_INSTALL_PATH "~/Software/ffmpeg_build")
endif (APPLE)
find_package(FFmpeg REQUIRED)
find_package(Threads REQUIRED)
list(APPEND link_libraries FFmpeg::FFmpeg Threads::Threads)

# pybind 11
find_package(pybind11)
if (NOT pybind11_FOUND)
    GitHelper(pybind11 https://github.com/pybind/pybind11.git v2.9.2 TRUE "" "")
endif ()
GitHelper(spdlog https://github.com/gabime/spdlog.git v1.10.0 TRUE "" "spdlog")

# # sources
# list(APPEND sources
#     src/ffutils/common.cpp
#     src/ffutils/video_reader.cpp
#     # src/ffutils/video_writer.cpp
# )

list(APPEND sources
    common.cpp
    video_reader.cpp
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
