cmake_minimum_required(VERSION 3.17)
project(ffms_python)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# include cmake utils
list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")
include(cmake/Fetch.cmake)  # GitHelper

# ffms2
add_subdirectory(ffms2)
list(APPEND link_libraries ffms2)

# nanobind and Python
GitHelper(nanobind https://github.com/wjakob/nanobind.git master TRUE "" "")
set(Python_FIND_FRAMEWORK LAST) # Prefer Brew/Conda to Apple framework python
find_package(Python 3.8 COMPONENTS Interpreter Development Development.Module REQUIRED)

# add sources
list(APPEND sources src/video_reader.cpp)

nanobind_add_module(ffms ${headers} ${sources} src/bind.cpp)
target_include_directories(ffms PRIVATE ${include_directories})
target_link_directories   (ffms PRIVATE ${link_directories})
target_link_libraries     (ffms PRIVATE ${link_libraries})
target_compile_definitions(ffms PRIVATE ${compile_definitions} ${definitions})

# # build cpp test
# add_executable            (test_cpp ${headers} ${sources} src/main.cpp)
# target_include_directories(test_cpp PUBLIC ${include_directories})
# target_link_directories   (test_cpp PUBLIC ${link_directories})
# target_link_libraries     (test_cpp PUBLIC ${link_libraries})
# target_compile_definitions(test_cpp PUBLIC ${compile_definitions} ${definitions})
