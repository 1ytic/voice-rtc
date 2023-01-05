a:
	rm -rf build
	cmake -S . -B build -DWRAPPER_INC="/home/ubuntu/voice-rtc/libwrapper" -DWRAPPER_LIB="/home/ubuntu/voice-rtc/libwrapper/build"  -DMKL_INCLUDE_DIR=/usr/include/mkl
	cmake --build build -j 2

b:
	cmake --build build -j 2

c:
	./build/server

d:
	cd /home/ubuntu/uWebSockets/uSockets
	make boringssl
	make WITH_BORINGSSL=1
