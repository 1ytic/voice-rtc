//
// Created by Ivan Sorokin on 02.11.2022.
//

#ifndef CREATE_SESSION_OBSERVER_H
#define CREATE_SESSION_OBSERVER_H

#include <api/peer_connection_interface.h>

#include <utility>

#include "wrapper.h"
#include "set_session_observer.h"


class CreateSessionDescriptionObserver : public webrtc::CreateSessionDescriptionObserver {
public:

    explicit CreateSessionDescriptionObserver(rtc::scoped_refptr<webrtc::PeerConnectionInterface> &pc, callback_t send): m_pc(pc), m_send(send) {}

    void OnSuccess(webrtc::SessionDescriptionInterface* desc) override;

    void OnFailure(webrtc::RTCError /*error*/) override {}

private:
    rtc::scoped_refptr<webrtc::SetSessionDescriptionObserver> m_observer;
    rtc::scoped_refptr<webrtc::PeerConnectionInterface> m_pc;
    callback_t m_send;
};

#endif //CREATE_SESSION_OBSERVER_H
