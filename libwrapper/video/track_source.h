#ifndef WEBRTC_WRAPPER_TRACK_SOURCE_H
#define WEBRTC_WRAPPER_TRACK_SOURCE_H

#include "refptr.h"
#include "ref_counted_base.h"

#include "api/media_stream_interface.h"
#include "media/base/adapted_video_track_source.h"
#include "rtc_base/thread.h"
#include "rtc_base/deprecated/recursive_critical_section.h"

#include "api/video/i420_buffer.h"
// libyuv from WebRTC repository for color conversion
#include "libyuv.h"

#include "rtc_base/logging.h"

/// View over an existing buffer representing a video frame encoded in ARGB
/// 32-bit-per-pixel format, in little endian order (B first, A last).
struct Argb32VideoFrame {
  /// Width of the video frame, in pixels.
  std::uint32_t width_;

  /// Height of the video frame, in pixels.
  std::uint32_t height_;

  /// Pointer to the raw contiguous memory block holding the video frame data.
  /// The size of the buffer is at least (|stride_| * |height_|) bytes.
  const void* argb32_data_;

  /// Stride in bytes between two consecutive rows in the ARGB buffer.
  /// This is always greater than or equal to |width_|.
  std::int32_t stride_;
};

/// Adapter to bridge a video track source to the underlying core
/// implementation.
struct CustomTrackSourceAdapter : public rtc::AdaptedVideoTrackSource {
  void DispatchFrame(const webrtc::VideoFrame& frame) { OnFrame(frame); }

  // VideoTrackSourceInterface
  bool is_screencast() const override { return false; }
  absl::optional<bool> needs_denoising() const override {
    return absl::nullopt;
  }

  // MediaSourceInterface
  SourceState state() const override { return state_; }
  bool remote() const override { return false; }

  SourceState state_ = SourceState::kInitializing;
};

enum class Result : std::uint32_t {
  /// The operation was successful.
  kSuccess = 0,

  //
  // Generic errors
  //

  /// A parameter passed to the API function was invalid.
  kInvalidParameter = 0x80000001,

  /// The operation cannot be performed in the current state.
  kInvalidOperation = 0x80000002,

  /// A call was made to an API function on the wrong thread.
  /// This is generally related to platforms with thread affinity like UWP.
  kWrongThread = 0x80000003,

  /// An object was not found.
  kNotFound = 0x80000004,

  /// An interop handle referencing a native object instance is invalid,
  /// although the API function was expecting a valid object.
  kInvalidNativeHandle = 0x80000005,

  /// The API object is not initialized, and cannot as a result perform the
  /// given operation.
  kNotInitialized = 0x80000006,
};

class ExternalVideoTrackSource;

/// Frame request for an external video source producing video frames encoded in
/// ARGB 32-bit-per-pixel format.
struct Argb32VideoFrameRequest {
  /// Video track source the request is related to.
  ExternalVideoTrackSource& track_source_;

  /// Video frame timestamp, in milliseconds.
  std::int64_t timestamp_ms_;

  /// Unique identifier of the request.
  const std::uint32_t request_id_;

  /// Complete the request by making the track source consume the given video
  /// frame and have it deliver the frame to all its video tracks.
  Result CompleteRequest(const Argb32VideoFrame& frame_view);
};

/// Custom video source producing video frames encoded in ARGB 32-bit-per-pixel
/// format.
class Argb32ExternalVideoSource {
 public:
  /// Produce a video frame for a request initiated by an external track source.
  ///
  /// This callback is invoked automatically by the track source whenever a new
  /// video frame is needed (pull model). The custom video source implementation
  /// must either return an error, or produce a new video frame and call the
  /// |CompleteRequest()| request on the |track_source| object, passing the
  /// |request_id| of the current request being completed.
  virtual Result FrameRequested(Argb32VideoFrameRequest& frame_request) {
    return Result::kSuccess;
  };
  virtual Result ContextInit() {
    return Result::kSuccess;
  };
  virtual Result ContextFree() {
    return Result::kSuccess;
  };
};

/// Adapter for the frame buffer of an external video track source,
/// to support various frame encodings in a unified way.
/// Buffer adapter for a 32-bit ARGB video frame.
class Argb32BufferAdapter {
 public:
  Argb32BufferAdapter(std::shared_ptr<Argb32ExternalVideoSource> video_source)
      : video_source_(video_source) {}
  /// Request a new video frame with the specified request ID.
  Result RequestFrame(ExternalVideoTrackSource& track_source,
                      std::uint32_t request_id,
                      std::int64_t timestamp_ms) noexcept {
    // Request a single ARGB32 frame
    Argb32VideoFrameRequest request{track_source, timestamp_ms, request_id};
    return video_source_->FrameRequested(request);
  }
  Result ContextInit() {
    return video_source_->ContextInit();
  };
  Result ContextFree() {
    return video_source_->ContextFree();
  };
  /// Allocate a new video frame buffer with a video frame received from a
  /// fulfilled frame request.
  rtc::scoped_refptr<webrtc::VideoFrameBuffer> FillBuffer(const Argb32VideoFrame& frame_view) {
    // Check that the input frame fits within the constraints of chroma
    // downsampling (width and height multiple of 2).
    uint32_t width = frame_view.width_;
    uint32_t height = frame_view.height_;
    // Create I420 buffer
    rtc::scoped_refptr<webrtc::I420Buffer> buffer = webrtc::I420Buffer::Create(width, height);
    // Convert to I420 and copy to buffer
    libyuv::ARGBToI420(
      (const uint8_t*)frame_view.argb32_data_, frame_view.stride_,
      buffer->MutableDataY(), buffer->StrideY(),
      buffer->MutableDataU(), buffer->StrideU(),
      buffer->MutableDataV(), buffer->StrideV(),
      width, height
    );
    return buffer;
  }
 private:
  std::shared_ptr<Argb32ExternalVideoSource> video_source_;
};

/// Video track source acting as an adapter for an external source of raw
/// frames.
class ExternalVideoTrackSource : public RefCountedBase {
 public:
  using SourceState = webrtc::MediaSourceInterface::SourceState;

  /// Helper to create an external video track source from a custom ARGB32 video
  /// frame request callback.
  static RefPtr<ExternalVideoTrackSource> createFromArgb32(
      std::shared_ptr<Argb32ExternalVideoSource> video_source);

  ~ExternalVideoTrackSource() override;

  /// Finish the creation of the video track source, and start capturing.
  /// See |mrsExternalVideoTrackSourceFinishCreation()| for details.
  void FinishCreation();

  /// Start the video capture. This will begin to produce video frames and start
  /// calling the video frame callback.
  void StartCapture();

  /// Complete a given video frame request with the provided ARGB32 frame.
  /// The caller must know the source expects an ARGB32 frame; there is no check
  /// to confirm the source is I420A-based or ARGB32-based.
  Result CompleteRequest(uint32_t request_id,
                         int64_t timestamp_ms,
                         const Argb32VideoFrame& frame);

  /// Stop the video capture. This will stop producing video frames.
  void StopCapture();

  /// Shutdown the source and release the buffer adapter and its callback.
  void Shutdown() noexcept;

  CustomTrackSourceAdapter* GetSourceImpl() const {
    return (CustomTrackSourceAdapter*)source_.get();
  }

 protected:

  ExternalVideoTrackSource(std::shared_ptr<Argb32ExternalVideoSource> video_source);

  void OnMessage();

  std::unique_ptr<Argb32BufferAdapter> adapter_;
  std::unique_ptr<rtc::Thread> capture_thread_;
  rtc::scoped_refptr<webrtc::VideoTrackSourceInterface> source_;

  /// Collection of pending frame requests
  // TODO : circular buffer to avoid alloc
  std::deque<std::pair<uint32_t, int64_t>> pending_requests_ RTC_GUARDED_BY(request_lock_);

  /// Next available ID for a frame request
  uint32_t next_request_id_ RTC_GUARDED_BY(request_lock_){};

  /// Lock for frame requests
  rtc::RecursiveCriticalSection request_lock_;
};

#endif //WEBRTC_WRAPPER_TRACK_SOURCE_H
