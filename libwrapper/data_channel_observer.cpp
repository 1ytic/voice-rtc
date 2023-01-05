//
// Created by Ivan Sorokin on 02.11.2022.
//

#include "data_channel_observer.h"

// Callback for when the server receives a message on the data channel.
void DataChannelObserver::OnMessage(const webrtc::DataBuffer& buffer) {
    std::string data(buffer.data.data<char>(), buffer.data.size());
    RTC_LOG(LS_INFO) << data;
    // std::string str = "pong";
    // webrtc::DataBuffer resp(rtc::CopyOnWriteBuffer(str.c_str(), str.length()), false /* binary */);
    // data_channel->Send(buffer);
    RTC_LOG(LS_INFO) << "OnDataChannelMessage";
}
