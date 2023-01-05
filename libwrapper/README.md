### Build WebRTC

```shell
gn_args='is_debug=false
rtc_include_tests=false
target_os="linux"
use_glib=true
rtc_use_h264=false
rtc_enable_libevent=false
libcxx_is_shared=true
rtc_use_dummy_audio_file_devices=true
rtc_include_pulse_audio=false' &&\
gn gen out/Default --args="${gn_args}"

ninja -C out/Default buildtools/third_party/libc++:libc++ -v &&\
sudo cp out/Default/libc++.so /usr/lib/ &&\
ninja -C out/Default -v &&\
cd .. && mv src webrtc
```

### Build libwrapper

```shell
rm -rf build &&\
cmake -S . -B build -DVANILLA_WEBRTC_SRC="/home/ubuntu/webrtc-build" -DCMAKE_TOOLCHAIN_FILE="cmake/LinuxClang.cmake" &&\
cmake --build build
```

https://blog.sigsegowl.xyz/opengl-offscreen-software-rendering-on-a-server/

## OpenGL libraries
https://stackoverflow.com/a/21946983
- **gl**: This is the base header file for OpenGL version 1.1. That means if you want to use any functionality beyond version 1.1, you have to add any extension library on this.
- **glew**: OpenGL Extension Wrangler Library. This is a cross-platform library for loading OpenGL extended functionality. When you initialize this library, it will check your platform and graphic card at run-time to know what functionality can be used in your program.
- **glu**: This is OpenGL utilities library, which has been not updated for long time. Don't need to use this header file.
glut: OpenGL Utility Toolkit for Windowing API. This is good for small to medium size OpenGL program. If you need more sophisticated windowing libraries, use native window system toolkits like GTK or Qt for linux machines.
- **glfw**: OpenGL Frame Work. Another multi-platform library for creating windows and handling events. FreeGlut can be used as an alternative. glfw is designed for game development.
- **glm**: OpenGL Mathematics. It helps implementing vectors and matrices operations.