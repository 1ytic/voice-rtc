cmake_minimum_required(VERSION 3.21)
project(server)

include(GNUInstallDirs)
include(CheckCXXCompilerFlag)

set(CMAKE_CXX_STANDARD 17)

set(AVAILABLE_INFERENCE_BACKENDS fbgemm)
set(W2L_INFERENCE_BACKEND fbgemm CACHE STRING "Inference backend library")
set(FBGEMM_SOURCE_DIR "/home/ubuntu/FBGEMM")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/")

add_executable(server main.cpp)

include_directories("/home/ubuntu/uWebSockets/src")
include_directories("/home/ubuntu/uWebSockets/uSockets/src")
include_directories("/home/ubuntu/flashlight")
include_directories("../libwrapper")

include(Buildcereal)

include_directories(${cereal_INCLUDE_DIRS})
include_directories(build/cereal/src/cereal/include/cereal/external)
include_directories(${CMAKE_CURRENT_LIST_DIR})

add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/inference/common)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/inference/module)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/inference/decoder)

find_package(Filesystem REQUIRED)
find_library(WRAPPER_LIBRARY wrapper HINTS ${WRAPPER_LIB})
find_package(flashlight CONFIG REQUIRED)

target_include_directories(server PRIVATE ${WRAPPER_INC})

target_link_libraries(server
        ${WRAPPER_LIBRARY}
        /home/ubuntu/uWebSockets/uSockets/uSockets.a
        /home/ubuntu/uWebSockets/uSockets/boringssl/build/ssl/libssl.a
        /home/ubuntu/uWebSockets/uSockets/boringssl/build/crypto/libcrypto.a
        OSMesa
        GLU
        streaming_inference_modules_nn
        streaming_inference_modules_feature
        streaming_inference_decoder
        flashlight::fl_pkg_speech)
