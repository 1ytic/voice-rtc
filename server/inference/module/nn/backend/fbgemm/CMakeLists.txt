cmake_minimum_required(VERSION 3.5.1)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/cmake)

# Library and include dirs
set(fbgemm_LIBRARIES
  "${FBGEMM_SOURCE_DIR}/build/${CMAKE_STATIC_LIBRARY_PREFIX}fbgemm${CMAKE_STATIC_LIBRARY_SUFFIX}"
  "${FBGEMM_SOURCE_DIR}/build/cpuinfo/${CMAKE_STATIC_LIBRARY_PREFIX}cpuinfo${CMAKE_STATIC_LIBRARY_SUFFIX}"
  "${FBGEMM_SOURCE_DIR}/build/cpuinfo/deps/clog/${CMAKE_STATIC_LIBRARY_PREFIX}clog${CMAKE_STATIC_LIBRARY_SUFFIX}"
)

set(fbgemm_INCLUDE_DIRS
  ${FBGEMM_SOURCE_DIR}/include
  ${FBGEMM_SOURCE_DIR}/third_party/asmjit/src
  ${FBGEMM_SOURCE_DIR}/third_party/cpuinfo/include
)

add_library(streaming_inference_modules_nn_backend
  ${CMAKE_CURRENT_LIST_DIR}/Conv1dFbGemm.cpp
  ${CMAKE_CURRENT_LIST_DIR}/LinearFbGemm.cpp
  ${CMAKE_CURRENT_LIST_DIR}/PackedGemmMatrixFP16.cpp
)

set_target_properties(
  streaming_inference_modules_nn_backend
  PROPERTIES
    LINKER_LANGUAGE CXX
)

add_dependencies(streaming_inference_modules_nn_backend cereal)

target_link_libraries(streaming_inference_modules_nn_backend
  PUBLIC
    streaming_inference_modules
    ${fbgemm_LIBRARIES}
)

target_include_directories(streaming_inference_modules_nn_backend
  PUBLIC
    ${fbgemm_INCLUDE_DIRS}
    ${cereal_INCLUDE_DIRS}
)

set(BACKEND_FOUND true)
