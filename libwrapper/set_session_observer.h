//
// Created by Ivan Sorokin on 02.11.2022.
//

#ifndef WEBRTC_WRAPPER_SET_SESSION_OBSERVER_H
#define WEBRTC_WRAPPER_SET_SESSION_OBSERVER_H

#include <api/peer_connection_interface.h>


class SetSessionDescriptionObserver : public webrtc::SetSessionDescriptionObserver {
public:
    void OnSuccess() override {
        RTC_LOG(LS_INFO) << "Success setting session description";
    }
    void OnFailure(webrtc::RTCError error) override {
        RTC_LOG(LS_ERROR) << "Error setting session description: "
                            << error.message();
    }
};

struct SetRemoteSessionDescObserver : public webrtc::SetRemoteDescriptionObserverInterface {
public:
    void OnSetRemoteDescriptionComplete(webrtc::RTCError error) override {
        if (error.ok()) {
            RTC_LOG(LS_INFO) << "Remote description successfully set.";
        } else {
            RTC_LOG(LS_ERROR) << "Error setting remote description: "
                              << error.message();
        }
    }
};

#endif //WEBRTC_WRAPPER_SET_SESSION_OBSERVER_H
