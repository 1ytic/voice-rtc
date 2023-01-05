#ifndef WRAPPER_LIBRARY_H
#define WRAPPER_LIBRARY_H

#include <stddef.h>
#include <stdint.h>

using callback_t = void(*)(const char * payload);
using callback2_t = void(*)(const int16_t* audio_data, size_t data_size, float *outputs);

class ConnectionWrapper {
public:
    ConnectionWrapper();
    void Join();
    void Quit();
    void CreateConnection(const char *sdp, callback_t send, callback2_t send2);
    void AddCandidate(const char *sdp_mid, int sdp_mline_index, const char *candidate);
};

#endif //WRAPPER_LIBRARY_H
