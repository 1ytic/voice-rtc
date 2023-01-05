#include "track_source.h"
#include "rtc_base/logging.h"


constexpr const size_t kMaxPendingRequestCount = 64;

RefPtr<ExternalVideoTrackSource> ExternalVideoTrackSource::createFromArgb32(
    std::shared_ptr<Argb32ExternalVideoSource> video_source) {
  // Note: Video track sources always start already capturing; there is no
  // start/stop mechanism at the track level in WebRTC. A source is either being
  // initialized, or is already live. However because of wrappers and interop
  // this step is delayed until |FinishCreation()| is called by the wrapper.
  return new ExternalVideoTrackSource(video_source);
}

ExternalVideoTrackSource::ExternalVideoTrackSource(
    std::shared_ptr<Argb32ExternalVideoSource> video_source)
    : adapter_(std::make_unique<Argb32BufferAdapter>(video_source)),
      capture_thread_(rtc::Thread::Create()),
      source_(new rtc::RefCountedObject<CustomTrackSourceAdapter>()) {
  capture_thread_->SetName("ExternalVideoTrackSource capture thread", this);
}

ExternalVideoTrackSource::~ExternalVideoTrackSource() {
  StopCapture();
}

void ExternalVideoTrackSource::FinishCreation() {
  StartCapture();
}

void ExternalVideoTrackSource::StartCapture() {
  // Check if |Shutdown()| was called, in which case the source cannot restart.
  if (!adapter_) {
    return;
  }

  RTC_LOG(LS_INFO) << "Starting capture for external video track source ";

  // Start capture thread
  GetSourceImpl()->state_ = SourceState::kLive;
  pending_requests_.clear();
  capture_thread_->Start();

  capture_thread_->BlockingCall([&] {
      RTC_LOG(LS_WARNING) << "Thread: capture_thread";
  });

  capture_thread_->BlockingCall([&]{adapter_->ContextInit();});

  // Schedule first frame request for 10ms from now
  capture_thread_->PostDelayedTask([&]{OnMessage();}, webrtc::TimeDelta::Millis(10));
}

Result ExternalVideoTrackSource::CompleteRequest(
    uint32_t request_id,
    int64_t timestamp_ms,
    const Argb32VideoFrame& frame_view) {
  // Validate pending request ID and retrieve frame timestamp
  int64_t timestamp_ms_original = -1;
  {
    rtc::CritScope lock(&request_lock_);
    for (auto it = pending_requests_.begin(); it != pending_requests_.end(); ++it) {
      if (it->first == request_id) {
        timestamp_ms_original = it->second;
        // Remove outdated requests, including current one
        ++it;
        pending_requests_.erase(pending_requests_.begin(), it);
        break;
      }
    }
    if (timestamp_ms_original < 0) {
      return Result::kInvalidParameter;
    }
  }

  // Apply user override if any
  if (timestamp_ms != timestamp_ms_original) {
    timestamp_ms = timestamp_ms_original;
  }

  // Create and dispatch the video frame
  webrtc::VideoFrame frame{
      webrtc::VideoFrame::Builder()
          .set_video_frame_buffer(adapter_->FillBuffer(frame_view))
          .set_timestamp_ms(timestamp_ms)
          .build()};
  GetSourceImpl()->DispatchFrame(frame);
  return Result::kSuccess;
}

void ExternalVideoTrackSource::StopCapture() {
  CustomTrackSourceAdapter* const src = GetSourceImpl();
  if (src->state_ != SourceState::kEnded) {
    RTC_LOG(LS_INFO) << "Stopping capture for external video track source";
    capture_thread_->BlockingCall([&]{adapter_->ContextFree();});
    capture_thread_->Stop();
    src->state_ = SourceState::kEnded;
  }
  pending_requests_.clear();
}

void ExternalVideoTrackSource::Shutdown() noexcept {
  StopCapture();
  adapter_ = nullptr;
}

// Note - This is called on the capture thread only.
void ExternalVideoTrackSource::OnMessage() {
  const int64_t now = rtc::TimeMillis();

  // Request a frame from the external video source
  uint32_t request_id = 0;
  {
    rtc::CritScope lock(&request_lock_);
    // Discard an old request if no space available. This allows restarting
    // after a long delay, otherwise skipping the request generally also
    // prevent the user from calling CompleteFrame() to make some space for
    // more. The queue is still useful for just-in-time or short delays.
    if (pending_requests_.size() >= kMaxPendingRequestCount) {
      pending_requests_.erase(pending_requests_.begin());
    }
    request_id = next_request_id_++;
    pending_requests_.emplace_back(request_id, now);
  }
  adapter_->RequestFrame(*this, request_id, now);

  // Schedule a new request for 30ms from now
  capture_thread_->PostDelayedTask([&]{OnMessage();}, webrtc::TimeDelta::Millis(30));
}

Result Argb32VideoFrameRequest::CompleteRequest(const Argb32VideoFrame& frame_view) {
  auto impl = static_cast<ExternalVideoTrackSource*>(&track_source_);
  return impl->CompleteRequest(request_id_, timestamp_ms_, frame_view);
}
