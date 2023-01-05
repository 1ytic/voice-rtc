//
// Created by Ivan Sorokin on 02.11.2022.
//

#ifndef WEBRTC_WRAPPER_DATA_CHANNEL_OBSERVER_H
#define WEBRTC_WRAPPER_DATA_CHANNEL_OBSERVER_H

#include <api/peer_connection_interface.h>


class DataChannelObserver : public webrtc::DataChannelObserver {
public:

    void OnStateChange() override {}

    void OnMessage(const webrtc::DataBuffer& buffer) override;

    void OnBufferedAmountChange(uint64_t /* previous_amount */) override {}

};

#endif //WEBRTC_WRAPPER_DATA_CHANNEL_OBSERVER_H
