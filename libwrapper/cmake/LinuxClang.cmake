set(CMAKE_SYSTEM_NAME Linux)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++ -Wall -Wextra -Wpedantic")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -stdlib=libc++")

set(CMAKE_C_COMPILER ${VANILLA_WEBRTC_SRC}/webrtc/third_party/llvm-build/Release+Asserts/bin/clang)
set(CMAKE_CXX_COMPILER ${VANILLA_WEBRTC_SRC}/webrtc/third_party/llvm-build/Release+Asserts/bin/clang++)

include_directories(SYSTEM ${VANILLA_WEBRTC_SRC}/webrtc/buildtools/third_party/libc++/trunk/include /home/ubuntu/webrtc-build/webrtc/third_party/llvm-build/Release+Asserts/lib/clang/14.0.0/include/ /home/ubuntu/webrtc-build/webrtc/buildtools/third_party/libc++/ /home/ubuntu/webrtc-build/webrtc/buildtools/third_party/libc++/trunk/include)
