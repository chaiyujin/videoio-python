cmake_minimum_required(VERSION 3.17)
project(ffutils)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
SET(CMAKE_POSITION_INDEPENDENT_CODE ON)

# include cmake utils
list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

# ffmpeg from brew
if (APPLE)
    set(FFmpeg_INSTALL_PATH "/usr/local/Cellar/ffmpeg/5.0.1")
endif (APPLE)
find_package(FFmpeg REQUIRED)
find_package(Threads REQUIRED)
list(APPEND link_libraries FFmpeg::FFmpeg Threads::Threads)

# pybind 11
find_package(pybind11 REQUIRED)

# fmt
add_subdirectory(ext/fmt)
list(APPEND link_libraries fmt)

# sources
list(APPEND sources
    src/ffutils/common.cpp
    src/ffutils/video_reader.cpp
)

pybind11_add_module       (ffutils ${headers} ${sources} src/pybind.cpp)
target_include_directories(ffutils PRIVATE ${include_directories})
target_link_directories   (ffutils PRIVATE ${link_directories})
target_link_libraries     (ffutils PRIVATE ${link_libraries})
target_compile_definitions(ffutils PRIVATE ${compile_definitions} ${definitions})

# add_executable(read_video src/main.cpp ${sources})
# target_include_directories(read_video PUBLIC ${include_directories})
# target_link_directories   (read_video PUBLIC ${link_directories})
# target_link_libraries     (read_video PUBLIC ${link_libraries})
# target_compile_definitions(read_video PUBLIC ${compile_definitions} ${definitions})
