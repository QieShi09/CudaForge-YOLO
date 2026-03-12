cmake_minimum_required(VERSION 3.18)
project(ExampleCuda LANGUAGES C CXX CUDA)

set(CMAKE_CXX_STANDARD 17)
enable_language(CUDA)

add_library(example_cuda STATIC
    nv12_to_rgb.cu
    letterbox.cu
)
set_target_properties(example_cuda PROPERTIES
    CUDA_SEPARABLE_COMPILATION ON
)

target_include_directories(example_cuda PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
