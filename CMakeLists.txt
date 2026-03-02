cmake_minimum_required(VERSION 3.22)

# --- 1. 路径硬锁定 (必须放在 project 之前) ---
set(CUDA_HOME "/usr/local/cuda-13.1")
set(CMAKE_CUDA_COMPILER "${CUDA_HOME}/bin/nvcc")

# 强制设置搜索路径，屏蔽系统 11.5
set(CMAKE_PREFIX_PATH ${CUDA_HOME} ${CMAKE_PREFIX_PATH})
set(CUDA_TOOLKIT_ROOT_DIR ${CUDA_HOME})
set(CUDAToolkit_ROOT ${CUDA_HOME})

project(CudaForge-YOLO LANGUAGES CXX CUDA)

# --- 2. 编译标准与架构对齐 ---
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CUDA_STANDARD 17)
set(CMAKE_CUDA_ARCHITECTURES 120) # RTX 4060 (Ada Lovelace)

# --- 3. 手动定义 CUDA 关键变量 (跳过 FindCUDAToolkit 的不靠谱探测) ---
set(CUDA_INCLUDE_DIRS "${CUDA_HOME}/include")
set(CUDA_LIBRARIES "${CUDA_HOME}/lib64/libcudart.so")
set(CUDA_cublas_LIBRARY "${CUDA_HOME}/lib64/libcublas.so")

# --- 4. 查找 OpenCV ---
# 先指定 OpenCV 可能需要的 CUDA 路径，防止它在内部报错
set(CUDA_DIR ${CUDA_HOME})
find_package(OpenCV REQUIRED)

# --- 5. 手动指定 TensorRT 路径 ---
set(TRT_DIR "/home/zzx/TensorRT-10.14.1")
set(TRT_INCLUDE_DIRS "${TRT_DIR}/include")
set(TRT_LIB_DIRS "${TRT_DIR}/lib")

# --- 6. 查找 Qt6 和 FFMPEG ---
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Gui OpenGLWidgets)
find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET libavcodec libavformat libavutil libswscale libavdevice)
add_compile_definitions(HAVE_AVDEVICE)

# --- 7. 源码管理 ---
file(GLOB_RECURSE CORE_SOURCES "src/core/*.cpp" "src/core/*.hpp")
file(GLOB_RECURSE ENGINE_SOURCES "src/engine/*.cpp" "src/engine/*.hpp")
file(GLOB_RECURSE KERNEL_SOURCES "src/kernels/*.cu" "src/kernels/*.cuh")
file(GLOB_RECURSE VIDEO_SOURCES "src/video/*.cpp" "src/video/*.hpp")
file(GLOB_RECURSE WIDGETS_SOURCES "src/widgets/*.cpp" "src/widgets/*.h" "src/widgets/*.hpp")

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

#qt_standard_project_setup()

# --- 8. 生成可执行文件 ---
qt_add_executable(CudaForge-YOLO
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    src/widgets/AdvancedSettingsDialog.ui
    ${CORE_SOURCES}
    ${ENGINE_SOURCES}
    ${KERNEL_SOURCES}
    ${VIDEO_SOURCES}
    ${WIDGETS_SOURCES}
)

# --- 9. 包含路径 (头文件) ---
target_include_directories(CudaForge-YOLO PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core
    ${CMAKE_CURRENT_SOURCE_DIR}/src/engine
    ${CMAKE_CURRENT_SOURCE_DIR}/src/kernels
    ${CMAKE_CURRENT_SOURCE_DIR}/src/video
    ${CMAKE_CURRENT_SOURCE_DIR}/src/widgets
    ${OpenCV_INCLUDE_DIRS}
    ${TRT_INCLUDE_DIRS}
    ${CUDA_INCLUDE_DIRS}   # 使用我们手动定义的 13.1 路径
    ${FFMPEG_INCLUDE_DIRS}
)

# --- 10. 链接库 (二进制) ---
target_link_directories(CudaForge-YOLO PRIVATE ${TRT_LIB_DIRS})

target_link_libraries(CudaForge-YOLO
    PRIVATE
    Qt::Core
    Qt::Widgets
    Qt::Gui
    Qt::OpenGLWidgets
    ${OpenCV_LIBS}
    ${CUDA_LIBRARIES}
    ${CUDA_cublas_LIBRARY}
    nvinfer
    nvonnxparser
    PkgConfig::FFMPEG
)

target_compile_definitions(CudaForge-YOLO PRIVATE QT_NO_KEYWORDS)
set_target_properties(CudaForge-YOLO PROPERTIES INSTALL_RPATH_USE_LINK_PATH TRUE)
add_compile_options(-Wno-deprecated-declarations)

# 针对 CUDA 文件的特定编译选项 (屏蔽 TensorRT 警告)
if(CMAKE_CUDA_COMPILER)
    target_compile_options(CudaForge-YOLO PRIVATE $<$<COMPILE_LANGUAGE:CUDA>:-w>)
endif()
