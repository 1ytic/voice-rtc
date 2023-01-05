//
// Created by Ivan Sorokin on 02.11.2022.
//

#include "create_session_observer.h"

#include <rapidjson/writer.h>
#include <rapidjson/document.h>


// Callback for when the answer is created. This sends the answer back to the client.
void CreateSessionDescriptionObserver::OnSuccess(webrtc::SessionDescriptionInterface *desc) {
    RTC_LOG(LS_INFO) << "OnAnswerCreated";
    m_observer = new rtc::RefCountedObject<SetSessionDescriptionObserver>();
    m_pc->SetLocalDescription(m_observer.get(), desc);
    std::string offer_string;
    desc->ToString(&offer_string);
    rapidjson::Document message_object;
    message_object.SetObject();
    message_object.AddMember("type", "answer", message_object.GetAllocator());
    rapidjson::Value sdp_value;
    sdp_value.SetString(rapidjson::StringRef(offer_string.c_str()));
    rapidjson::Value message_payload;
    message_payload.SetObject();
    message_payload.AddMember("type", "answer", message_object.GetAllocator());
    message_payload.AddMember("sdp", sdp_value, message_object.GetAllocator());

    RTC_LOG(LS_INFO) << "Answer: " << offer_string;

    message_object.AddMember("payload", message_payload, message_object.GetAllocator());
    rapidjson::StringBuffer strbuf;
    rapidjson::Writer<rapidjson::StringBuffer> writer(strbuf);
    message_object.Accept(writer);
    std::string payload = strbuf.GetString();
    if (m_send) {
        m_send(payload.c_str());
    } else {
        RTC_LOG(LS_INFO) << "WARNING: empty callback";
    }
}
