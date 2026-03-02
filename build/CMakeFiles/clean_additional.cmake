# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "Debug")
  file(REMOVE_RECURSE
  "CMakeFiles/CudaForge-YOLO_autogen.dir/AutogenUsed.txt"
  "CMakeFiles/CudaForge-YOLO_autogen.dir/ParseCache.txt"
  "CudaForge-YOLO_autogen"
  )
endif()
