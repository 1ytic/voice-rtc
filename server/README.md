
### Build server

```shell
rm -rf build && cmake -S . -B build -DWRAPPER_INC="/home/ubuntu/voice-rtc/libwrapper" -DWRAPPER_LIB="/home/ubuntu/voice-rtc/libwrapper/build"  -DMKL_INCLUDE_DIR=/usr/include/mkl && cmake --build build -j 2
```

### Speech Recognition Model

https://github.com/flashlight/wav2letter/tree/main/recipes/streaming_convnets
https://research.facebook.com/publications/scaling-up-online-speech-recognition-using-convnets/
