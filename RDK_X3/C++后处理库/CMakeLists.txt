cmake_minimum_required(VERSION 3.10)
project(postprocess)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_BUILD_TYPE Release)

add_library(postprocess SHARED postprocess.cpp postprocess.h)

set_target_properties(postprocess PROPERTIES
    C_VISIBILITY_PRESET default
    CXX_VISIBILITY_PRESET default
    VISIBILITY_INLINES_HIDDEN OFF
)

set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/build)

