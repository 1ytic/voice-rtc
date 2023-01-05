a:
	rm -rf build
	cmake -S . -B build -DVANILLA_WEBRTC_SRC="/home/ubuntu/webrtc-build" -DCMAKE_TOOLCHAIN_FILE="cmake/LinuxClang.cmake"
	cmake --build build -j 2

b:
	cmake --build build -j 2