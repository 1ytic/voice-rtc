//
// Created by Ivan Sorokin on 02.11.2022.
//

#include "peer_connection_observer.h"

#include <rapidjson/writer.h>
#include <rapidjson/document.h>


void PeerConnectionObserver::OnAddStream(rtc::scoped_refptr<webrtc::MediaStreamInterface> stream) {
    RTC_LOG(LS_INFO) << "OnAddStream: " << stream->id();
    auto audioTracks = stream->GetAudioTracks();
    if (audioTracks.size() == 1) {
        auto track = audioTracks.at(0);
        RTC_LOG(LS_INFO) << "Create AudioSink: " << track->id();
        track->AddSink(m_audio_sink.get());
    }
}

// Callback for when the data channel is successfully created.
void PeerConnectionObserver::OnDataChannel(rtc::scoped_refptr<webrtc::DataChannelInterface> channel) {
    RTC_LOG(LS_INFO) << "OnDataChannel";
    m_data_channel = channel;
    m_audio_sink->m_send2 = m_send2;
    m_audio_sink->SetDataChannel(channel);
    m_data_channel->RegisterObserver(&m_data_channel_observer);
}

// Callback for when the STUN server responds with the ICE candidates.
void PeerConnectionObserver::OnIceCandidate(const webrtc::IceCandidateInterface* candidate) {
    
    rapidjson::Value candidate_value;
    rapidjson::Value sdp_mid_value;
    rapidjson::Value payload;
    rapidjson::Document message;

    std::string candidate_str;
    candidate->ToString(&candidate_str);

    candidate_value.SetString(rapidjson::StringRef(candidate_str.c_str()));
    sdp_mid_value.SetString(rapidjson::StringRef(candidate->sdp_mid().c_str()));

    payload.SetObject();
    payload.AddMember("candidate", candidate_value, message.GetAllocator());
    payload.AddMember("sdpMid", sdp_mid_value, message.GetAllocator());
    payload.AddMember("sdpMLineIndex", candidate->sdp_mline_index(), message.GetAllocator());

    message.SetObject();
    message.AddMember("type", "candidate", message.GetAllocator());
    message.AddMember("payload", payload, message.GetAllocator());

    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    message.Accept(writer);

    std::string json = buffer.GetString();
    m_send(json.c_str());
}
