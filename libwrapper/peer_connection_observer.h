//
// Created by Ivan Sorokin on 02.11.2022.
//

#ifndef PEER_CONNECTION_OBSERVER_H
#define PEER_CONNECTION_OBSERVER_H

#include <api/peer_connection_interface.h>
#include "wrapper.h"
#include "audio_sink.h"
#include "data_channel_observer.h"


class PeerConnectionObserver : public webrtc::PeerConnectionObserver {
public:

    PeerConnectionObserver(): m_audio_sink(std::make_shared<AudioSink>()), m_data_channel(nullptr) {}

    ~PeerConnectionObserver() {
        if (m_data_channel) {
            m_data_channel->UnregisterObserver();
            m_data_channel.release();
        }
        if (m_audio_sink) {
            m_audio_sink.reset();
        }
    }

    void OnAddStream(rtc::scoped_refptr<webrtc::MediaStreamInterface> stream) override;
    void OnDataChannel(rtc::scoped_refptr<webrtc::DataChannelInterface> channel) override;
    void OnIceCandidate(const webrtc::IceCandidateInterface* candidate) override;

    void OnRenegotiationNeeded() override {}
    void OnIceConnectionChange(webrtc::PeerConnectionInterface::IceConnectionState /* new_state */) override {}
    void OnIceGatheringChange(webrtc::PeerConnectionInterface::IceGatheringState /* new_state */) override {}
    void OnSignalingChange(webrtc::PeerConnectionInterface::SignalingState /* new_state */) override {}

    callback_t m_send;
    callback2_t m_send2;

    std::shared_ptr<AudioSink> m_audio_sink;

private:
    rtc::scoped_refptr<webrtc::DataChannelInterface> m_data_channel;
    DataChannelObserver m_data_channel_observer;
};

#endif //PEER_CONNECTION_OBSERVER_H
