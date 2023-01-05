#include "wrapper.h"

#include <memory>
#include <thread>
#include <utility>

#include <rtc_base/thread.h>
#include <rtc_base/ssl_adapter.h>

#include <api/peer_connection_interface.h>
#include <api/create_peerconnection_factory.h>
#include <api/task_queue/default_task_queue_factory.h>
#include <api/audio_codecs/audio_decoder_factory_template.h>
#include <api/audio_codecs/audio_encoder_factory_template.h>
#include <api/audio_codecs/opus/audio_decoder_opus.h>
#include <api/audio_codecs/opus/audio_encoder_opus.h>

#include <api/video_codecs/builtin_video_decoder_factory.h>
#include <api/video_codecs/builtin_video_encoder_factory.h>

#include <modules/audio_device/include/test_audio_device.h>

#include <modules/video_capture/video_capture.h>
#include <modules/video_capture/video_capture_factory.h>
#include <pc/video_track_source.h>

#include "peer_connection_observer.h"
#include "create_session_observer.h"
#include "set_session_observer.h"

#include "video/track_source.h"


rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> peer_connection_factory;
rtc::scoped_refptr<webrtc::PeerConnectionInterface> peer_connection;

std::unique_ptr<PeerConnectionObserver> ptr_connection_observer;
rtc::scoped_refptr<webrtc::SetRemoteDescriptionObserverInterface> ptr_dummy_observer;
rtc::scoped_refptr<webrtc::CreateSessionDescriptionObserver> ptr_create_observer;

std::thread webrtc_thread;
rtc::Thread *webrtc_thread_wrapper;
std::unique_ptr<rtc::Thread> networkThread;
std::unique_ptr<rtc::Thread> signalingThread;
std::unique_ptr<rtc::Thread> workerThread;

rtc::scoped_refptr<webrtc::VideoTrackInterface> video_track;
RefPtr<ExternalVideoTrackSource> video_track_source;


void MainThreadEntry() {

    RTC_LOG(LS_INFO) << "MainThreadEntry started";

    webrtc_thread_wrapper = rtc::ThreadManager::Instance()->WrapCurrentThread();

    rtc::InitializeSSL();

    networkThread = rtc::Thread::CreateWithSocketServer();
    signalingThread = rtc::Thread::Create();
    workerThread = rtc::Thread::Create();

    networkThread->SetName("network", nullptr);
    signalingThread->SetName("signaling", nullptr);
    workerThread->SetName("worker", nullptr);

    if (!networkThread->Start() || !signalingThread->Start() || !workerThread->Start()) {
        RTC_LOG(LS_ERROR) << "Thread started with errors";
    }

    webrtc_thread_wrapper->BlockingCall([&] {
        RTC_LOG(LS_WARNING) << "Thread: webrtc_thread_wrapper";
    });
    networkThread->BlockingCall([&] {
        RTC_LOG(LS_WARNING) << "Thread: networkThread";
    });
    signalingThread->BlockingCall([&] {
        RTC_LOG(LS_WARNING) << "Thread: signalingThread";
    });
    workerThread->BlockingCall([&] {
        RTC_LOG(LS_WARNING) << "Thread: workerThread";
    });

    std::unique_ptr<webrtc::TaskQueueFactory> queueFactory;
    rtc::scoped_refptr<webrtc::TestAudioDeviceModule> adm;
    rtc::scoped_refptr<webrtc::AudioProcessing> apm;
    rtc::scoped_refptr<webrtc::AudioDecoderFactory> audio_decoder_factory;
    rtc::scoped_refptr<webrtc::AudioEncoderFactory> audio_encoder_factory;

    workerThread->BlockingCall([&] {
        queueFactory = webrtc::CreateDefaultTaskQueueFactory();
        adm = webrtc::TestAudioDeviceModule::Create(
            queueFactory.get(),
            nullptr,
            webrtc::TestAudioDeviceModule::CreateDiscardRenderer(48000, 1)
        );
        auto builder = webrtc::AudioProcessingBuilder();
        webrtc::AudioProcessing::Config apm_config;
        //apm_config.gain_controller1.enabled = false;
        //apm_config.gain_controller2.enabled = false;
        builder.SetConfig(apm_config);
        apm = builder.Create();
    });

    audio_decoder_factory = webrtc::CreateAudioDecoderFactory<webrtc::AudioDecoderOpus>();
    audio_encoder_factory = webrtc::CreateAudioEncoderFactory<webrtc::AudioEncoderOpus>();

    peer_connection_factory = webrtc::CreatePeerConnectionFactory(
        networkThread.get(),
        workerThread.get(),
        signalingThread.get(),
        adm,
        audio_encoder_factory,
        audio_decoder_factory,
        webrtc::CreateBuiltinVideoEncoderFactory(),
        webrtc::CreateBuiltinVideoDecoderFactory(),
        nullptr,
        apm
    );
    
    ptr_connection_observer = std::make_unique<PeerConnectionObserver>();

    // Tracks need to be created from the worker thread
    workerThread->BlockingCall([&] {
        video_track_source = ExternalVideoTrackSource::createFromArgb32(ptr_connection_observer->m_audio_sink);
    });

    if (!video_track_source) {
        RTC_LOG(LS_ERROR) << "Failed to create video_track_source";
    }

    video_track_source->FinishCreation();

    // Create the video track
    video_track = peer_connection_factory->CreateVideoTrack("video", video_track_source->GetSourceImpl());
    if (!video_track) {
        RTC_LOG(LS_ERROR) << "Failed to create local video track from source.";
    }

    if (!video_track->enabled()) {
        RTC_LOG(LS_ERROR) << "video_track->enabled() - Failed";
    }

    webrtc_thread_wrapper->Run();

    if (peer_connection) {
        peer_connection->Close();
        peer_connection.release();
        ptr_dummy_observer.release();
        ptr_create_observer.release();
        ptr_connection_observer.reset();
    }

    peer_connection_factory.release();

    audio_decoder_factory.release();
    audio_encoder_factory.release();

    video_track_source->StopCapture();

    workerThread->BlockingCall([&] {
        video_track_source.release();
        apm.release();
        adm.release();
        queueFactory.reset();
    });

    if (networkThread) {
        networkThread->Quit();
        networkThread.reset();
        RTC_LOG(LS_INFO) << "Quit networkThread";
    }

    if (signalingThread) {
        signalingThread->Quit();
        signalingThread.reset();
        RTC_LOG(LS_INFO) << "Quit signalingThread";
    }

    if (workerThread) {
        workerThread->Quit();
        workerThread.reset();
        RTC_LOG(LS_INFO) << "Quit workerThread";
    }

    rtc::CleanupSSL();

    rtc::ThreadManager::Instance()->UnwrapCurrentThread();

    RTC_LOG(LS_WARNING) << "Finished main WebRTC thread!";
}


ConnectionWrapper::ConnectionWrapper() {

    rtc::LogMessage::LogToDebug(rtc::LS_WARNING);
    rtc::LogMessage::LogTimestamps();
    rtc::LogMessage::LogThreads();

    webrtc_thread = std::thread(MainThreadEntry);

    RTC_LOG(LS_INFO) << "WebRTC thread created";
}

void ConnectionWrapper::Join() {
    RTC_LOG(LS_INFO) << "Joining to WebRTC thread";
    webrtc_thread.join();
}

void ConnectionWrapper::Quit() {
    RTC_LOG(LS_WARNING) << "Quit Video thread";
    video_track_source->StopCapture();
    RTC_LOG(LS_WARNING) << "Quit WebRTC thread";
    webrtc_thread_wrapper->Quit();
    webrtc_thread.join();
}

void ConnectionWrapper::CreateConnection(const char *sdp, callback_t send, callback2_t send2) {

    webrtc::PeerConnectionInterface::IceServer ice_server;
    ice_server.uri = "stun:stun.l.google.com:19302";

    webrtc::PeerConnectionInterface::RTCConfiguration configuration;
	configuration.sdp_semantics = webrtc::SdpSemantics::kUnifiedPlan;
    configuration.servers.push_back(ice_server);

    ptr_connection_observer->m_send = send;
    ptr_connection_observer->m_send2 = send2;

    webrtc::PeerConnectionDependencies dependencies(ptr_connection_observer.get());

    auto peer_connection_result = peer_connection_factory->CreatePeerConnectionOrError(
        configuration,
        std::move(dependencies)
    );
    if (!peer_connection_result.ok()) {
        RTC_LOG(LS_ERROR) << "CreatePeerConnectionOrError - Failed";
    }
    RTC_LOG(LS_INFO) << "CreatePeerConnectionOrError - Ok";
    peer_connection = peer_connection_result.MoveValue();

    webrtc::DataChannelInit data_channel_config;
    data_channel_config.ordered = false;
    data_channel_config.maxRetransmits = 0;
    auto dc_result = peer_connection->CreateDataChannelOrError("dc", &data_channel_config);
    if (!dc_result.ok()) {
        RTC_LOG(LS_ERROR) << "CreateDataChannelOrError - Failed";
    }

    webrtc::SdpParseError error;
    auto desc_ptr = webrtc::CreateSessionDescription("offer", std::string(sdp), &error);
    std::unique_ptr<webrtc::SessionDescriptionInterface> session_description(desc_ptr);

    if (!session_description) {
        RTC_LOG(LS_ERROR) << "CreateSessionDescription - Failed: " << error.description.c_str();
    }

    ptr_dummy_observer = new rtc::RefCountedObject<SetRemoteSessionDescObserver>();

    peer_connection->SetRemoteDescription(std::move(session_description), std::move(ptr_dummy_observer));

    auto rtp_transceivers = peer_connection->GetTransceivers();
    RTC_LOG(LS_INFO) << "RTP transceiver: " << rtp_transceivers.size();
    for (auto&& transceiver : rtp_transceivers) {
        std::string name = transceiver->mid().value_or("UNKNOWN");
        RTC_LOG(LS_INFO) << "RTP transceiver " << name.c_str();
        if (name == "1") {
            if (!transceiver->SetDirectionWithError(webrtc::RtpTransceiverDirection::kSendOnly).ok()) {
                RTC_LOG(LS_ERROR) << "SetDirectionWithError - Failed";
            }
            rtc::scoped_refptr<webrtc::RtpSenderInterface> sender = transceiver->sender();
            if (!sender->SetTrack(video_track.get())) {
                RTC_LOG(LS_ERROR) << "SetTrack - Failed";
            }
            RTC_LOG(LS_INFO) << "SetTrack - Ok";
        }
    }

    ptr_create_observer = new rtc::RefCountedObject<CreateSessionDescriptionObserver>(peer_connection, send);

    webrtc::PeerConnectionInterface::RTCOfferAnswerOptions options;
    peer_connection->CreateAnswer(ptr_create_observer.get(), options);
}

void ConnectionWrapper::AddCandidate(const char *sdp_mid, int sdp_mline_index, const char *candidate) {
    webrtc::SdpParseError error;
    auto candidate_object = webrtc::CreateIceCandidate(
        std::string(sdp_mid),
        sdp_mline_index,
        std::string(candidate),
        &error);
    peer_connection->AddIceCandidate(candidate_object);
    RTC_LOG(LS_VERBOSE) << "IceCandidate added: " << std::string(sdp_mid) << ", " << sdp_mline_index << " = " << std::string(candidate);
}
