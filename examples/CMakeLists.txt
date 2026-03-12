cmake_minimum_required(VERSION 3.18)
project(CudaForgeExamples LANGUAGES C CXX CUDA)

set(CMAKE_CXX_STANDARD 17)

find_package(PkgConfig REQUIRED)
pkg_check_modules(AVFORMAT REQUIRED libavformat)
pkg_check_modules(AVCODEC REQUIRED libavcodec)
pkg_check_modules(AVUTIL REQUIRED libavutil)
pkg_check_modules(SWSCALE QUIET libswscale)

add_subdirectory(cuda)

add_executable(hw_decode_demo hw_decode_demo.cpp)
target_include_directories(hw_decode_demo PRIVATE ${AVFORMAT_INCLUDE_DIRS} ${AVCODEC_INCLUDE_DIRS} ${AVUTIL_INCLUDE_DIRS})
target_link_libraries(hw_decode_demo PRIVATE ${AVFORMAT_LIBRARIES} ${AVCODEC_LIBRARIES} ${AVUTIL_LIBRARIES})

add_executable(seek_demo seek_demo.cpp)
target_include_directories(seek_demo PRIVATE ${AVFORMAT_INCLUDE_DIRS} ${AVCODEC_INCLUDE_DIRS} ${AVUTIL_INCLUDE_DIRS})
target_link_libraries(seek_demo PRIVATE ${AVFORMAT_LIBRARIES} ${AVCODEC_LIBRARIES} ${AVUTIL_LIBRARIES})

add_executable(speed_demo speed_demo.cpp)
target_include_directories(speed_demo PRIVATE ${AVFORMAT_INCLUDE_DIRS} ${AVCODEC_INCLUDE_DIRS} ${AVUTIL_INCLUDE_DIRS})
target_link_libraries(speed_demo PRIVATE ${AVFORMAT_LIBRARIES} ${AVCODEC_LIBRARIES} ${AVUTIL_LIBRARIES})

add_executable(nv12_demo cuda/nv12_demo.cpp)
target_include_directories(nv12_demo PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/cuda)
target_link_libraries(nv12_demo PRIVATE example_cuda)

add_executable(letterbox_demo cuda/letterbox_demo.cpp)
target_include_directories(letterbox_demo PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/cuda)
target_link_libraries(letterbox_demo PRIVATE example_cuda)
