//
// Created by Ivan Sorokin on 02.11.2022.
//

#ifndef WEBRTC_WRAPPER_AUDIO_SINK_H
#define WEBRTC_WRAPPER_AUDIO_SINK_H

#include <api/peer_connection_interface.h>
#include <audio/remix_resample.h>
#include <common_audio/resampler/include/push_resampler.h>
#include "video/track_source.h"

#include <utility>
#include <limits>

#include <GL/osmesa.h>
#include <GL/glu.h>
#include "video/shader_utils.h"

#define WIDTH 800
#define HEIGHT 400

#define WIDTH0 8000 // 500 msec with 16kHz
#define WIDTH1 3201
#define WIDTH2 6400 // 8 seconds with 800Hz
#define WIDTH3 160 // recognition frames for 8 seconds


class AudioSink : public webrtc::AudioTrackSinkInterface, public Argb32ExternalVideoSource {
public:

    explicit AudioSink(): m_counter(0), recognition_thread_(rtc::Thread::Create()) {
        // Inputs frame for recognition
        m_inputs_frame.Mute();
        m_inputs_frame.num_channels_ = 1;
        m_inputs_frame.sample_rate_hz_ = 16000;
        m_inputs_frame.samples_per_channel_ = 160;
        // Render frame for video track
        m_render_frame.Mute();
        m_render_frame.num_channels_ = 1;
        m_render_frame.sample_rate_hz_ = 800;
        m_render_frame.samples_per_channel_ = 8;
        // Setup recognition thread
        recognition_thread_->SetName("Recognition thread", this);
        recognition_thread_->Start();
        recognition_thread_->BlockingCall([&] {
            RTC_LOG(LS_WARNING) << "Thread: recognition_thread";
        });
        std::memset(m_outputs, 0, WIDTH3 * sizeof(float));
        std::memset(m_offsets, 0, WIDTH3 * sizeof(float));
    }

    void SetDataChannel(rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel) {
        m_data_channel = std::move(data_channel);
    }

    void OnData(const void* src_data,
                int bits_per_sample,
                int sample_rate,
                size_t number_of_channels,
                size_t number_of_frames) override {

        RTC_LOG(LS_VERBOSE) << "OnData:" << bits_per_sample << "/" << sample_rate << "/" << number_of_channels << "/" << number_of_frames;

        if (bits_per_sample != 16) {
            return;
        }
        if (sample_rate != 48000) {
            return;
        }
        if (number_of_channels != 1) {
            return;
        }
        if (number_of_frames != 480) {
            return;
        }

        webrtc::voe::RemixAndResample((const int16_t*)src_data, number_of_frames, number_of_channels, sample_rate, &m_resampler, &m_inputs_frame);
        webrtc::voe::RemixAndResample((const int16_t*)src_data, number_of_frames, number_of_channels, sample_rate, &m_resampler, &m_render_frame);

        size_t n1 = m_inputs_frame.samples_per_channel_;
        size_t n2 = m_render_frame.samples_per_channel_;

        size_t m1 = WIDTH0 - n1;
        size_t m2 = WIDTH2 - n2;

        std::memmove(m_inputs, m_inputs + n1, m1 * sizeof(int16_t));
        std::memmove(m_points, m_points + n2, m2 * sizeof(int16_t));

        const int16_t* inputs_data = m_inputs_frame.data();
        const int16_t* render_data = m_render_frame.data();

        std::memcpy(&m_inputs[m1], inputs_data, n1 * sizeof(int16_t));
        std::memcpy(&m_points[m2], render_data, n2 * sizeof(int16_t));

        for (size_t i = 0; i < WIDTH3; i++) {
            m_offsets[i] -= 8.0 / 6400.0;
        }

        m_counter += 1;
        if (m_counter >= 50) {
            m_counter = 0;
            if (m_data_channel) {
                int16_t *inputs = (int16_t *)malloc(WIDTH0 * sizeof(int16_t));
                std::memcpy(inputs, m_inputs, WIDTH0 * sizeof(int16_t));
                recognition_thread_->PostTask([this, inputs, counter = m_counter](){RecogniseAudio(inputs, counter);});
                // SEND TO DC
                //std::ostringstream stringStream;
                //stringStream << "format:" << sample_rate << "/" << number_of_channels << "/" << number_of_frames;
                //std::string str = stringStream.str();
                //webrtc::DataBuffer resp(rtc::CopyOnWriteBuffer(str.c_str(), str.length()), false /* binary */);
                //m_data_channel->Send(resp);
            }
        }
    }

    void RecogniseAudio(int16_t *inputs, int counter) {
        const size_t n = 10;
        const size_t m = WIDTH3 - n;
        float outputs[n];
        m_send2(inputs, WIDTH0, outputs);
        free(inputs);
        std::memmove(m_outputs, m_outputs + n, m * sizeof(float));
        std::memmove(m_offsets, m_offsets + n, m * sizeof(float));
        std::memcpy(&m_outputs[m], outputs, n * sizeof(float));
        float diff = (m_counter - counter) * (8.0 / 6400.0);
        float shift0 = 600.0 / 6400.0;
        float shift = 400.0 / 6400.0;
        for (size_t i = 0; i < n; i++) {
            m_offsets[m + i] = - diff - shift0;
        }
        for (size_t i = 0; i < m; i++) {
            m_offsets[i] += shift;
        }
    }

    Result ContextInit() override {

        RTC_LOG(LS_WARNING) << "OSmesa major version: " << OSMESA_MAJOR_VERSION;
        RTC_LOG(LS_WARNING) << "OSmesa minor version: " << OSMESA_MINOR_VERSION;

        /* Create an RGBA-mode Off-Screen Mesa rendering context */
        ctx = OSMesaCreateContextExt(GL_RGBA, 32, 0, 0, nullptr);
        if (!ctx) {
            RTC_LOG(LS_ERROR) << "OSMesaCreateContext failed!";
            return Result::kNotInitialized;
        }
        /* Allocate the image buffer */
        buffer = (GLfloat *)malloc(WIDTH * HEIGHT * 4 * sizeof(GLfloat));
        if (!buffer) {
            RTC_LOG(LS_ERROR) << "Alloc image buffer failed!";
            return Result::kNotInitialized;
        }
        /* Bind the buffer to the context and make it current */
        if (!OSMesaMakeCurrent(ctx, buffer, GL_FLOAT, WIDTH, HEIGHT)) {
            RTC_LOG(LS_ERROR) << "OSMesaMakeCurrent failed!";
            return Result::kNotInitialized;
        }

        // Program 1

        program1 = create_program(
            "/home/ubuntu/voice-rtc/libwrapper/video/graph.v.glsl",
            "/home/ubuntu/voice-rtc/libwrapper/video/graph.f.glsl"
        );
        if (program1 == 0) {
            RTC_LOG(LS_ERROR) << "program1 failed!";
            return Result::kNotInitialized;
        }

        attribute_coord1d = get_attrib(program1, "coord1d");
        uniform_mytexture1 = get_uniform(program1, "mytexture");

        if (attribute_coord1d == -1 || uniform_mytexture1 == -1) {
            RTC_LOG(LS_ERROR) << "program1 wrong!";
            return Result::kNotInitialized;
        }

        // Program 2

        program2 = create_program(
            "/home/ubuntu/voice-rtc/libwrapper/video/graph2.v.glsl",
            "/home/ubuntu/voice-rtc/libwrapper/video/graph.f.glsl"
        );
        if (program2 == 0) {
            RTC_LOG(LS_ERROR) << "program2 failed!";
            return Result::kNotInitialized;
        }

        attribute_coord2d = get_attrib(program2, "coord2d");

        if (attribute_coord2d == -1) {
            RTC_LOG(LS_ERROR) << "program2 wrong!";
            return Result::kNotInitialized;
        }

        glUniform1i(uniform_mytexture1, 0);

        // Enable blending
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        glEnable(GL_POINT_SPRITE);
        glEnable(GL_VERTEX_PROGRAM_POINT_SIZE);

        {
            // Create the vertex buffer object
            glGenBuffers(1, &vbo1);
            glBindBuffer(GL_ARRAY_BUFFER, vbo1);
            // Fill it in just like an array
            GLfloat xpoints[WIDTH1];
            GLfloat xhalf = (WIDTH1 - 1) / 2.0;
            for (int i = 0; i < WIDTH1; i++) {
                xpoints[i] = (i - xhalf) / xhalf;
            }
            // Tell OpenGL to copy our array to the buffer object
            glBufferData(GL_ARRAY_BUFFER, sizeof xpoints, xpoints, GL_STATIC_DRAW);
        }

        {
            // Create the vertex buffer object
            glGenBuffers(1, &vbo2);
        }

        {
            GLfloat range[2];
            glGetFloatv(GL_ALIASED_POINT_SIZE_RANGE, range);
            fprintf(stderr, "Point sprite range: (%f, %f)\n", range[0], range[1]);
        }

        {
            GLint max_units;
            glGetIntegerv(GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS, &max_units);
            if (max_units < 5) {
                fprintf(stderr, "Your GPU does not have any vertex texture image units\n");
            } else {
                fprintf(stderr, "Vertex texture image units: %d\n", max_units);
            }
        }

        {
            GLint r, g, b, a;
            glGetIntegerv(GL_RED_BITS, &r);
            glGetIntegerv(GL_GREEN_BITS, &g);
            glGetIntegerv(GL_BLUE_BITS, &b);
            glGetIntegerv(GL_ALPHA_BITS, &a);
            printf("Channel sizes: %d %d %d %d\n", r, g, b, a);
        }

        return Result::kSuccess;
    }

    Result ContextFree() override {
        glDeleteProgram(program1);
        glDeleteProgram(program2);
        /* free the image buffer */
        free(buffer);
        /* destroy the context */
        OSMesaDestroyContext(ctx);
        return Result::kSuccess;
    }

    Result FrameRequested(Argb32VideoFrameRequest& frame_request) override {

        glClearColor(1.0, 1.0, 1.0, 1.0);
        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(program1);

        // Create our datapoints
        GLfloat ypoints[WIDTH2];
        for (int i = 0; i < WIDTH2; i++) {
            float y = (float) m_points[i] / 400.0;
            if (y > 1)
                y = 1;
            if (y < -1)
                y = -1;
            y = y / 2.0 + 0.5;
            ypoints[i] = y;
        }
        // Create texture
        GLuint texture_id;
        glActiveTexture(GL_TEXTURE0);
        glGenTextures(1, &texture_id);
        glBindTexture(GL_TEXTURE_2D, texture_id);
        // Set texture wrapping mode
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT); // GL_CLAMP_TO_EDGE
        // Set texture interpolation mode
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        // Upload texture with our datapoints
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, WIDTH2, 1, 0, GL_RED, GL_FLOAT, ypoints);

        // Draw using the vertices in our vertex buffer object
        glBindBuffer(GL_ARRAY_BUFFER, vbo1);
        glEnableVertexAttribArray(attribute_coord1d);
        glVertexAttribPointer(attribute_coord1d, 1, GL_FLOAT, GL_FALSE, 0, 0);
		glDrawArrays(GL_LINE_STRIP, 0, WIDTH1);

        glUseProgram(program2);

        struct point {
            GLfloat x;
            GLfloat y;
        };

        glBindBuffer(GL_ARRAY_BUFFER, vbo2);
        // Fill it in just like an array
        point xpoints[WIDTH3];
        GLfloat xhalf = (float)WIDTH3 / 2.0;
        for (int i = 0; i < WIDTH3; i++) {
            xpoints[i].x = (float)(i - xhalf) / xhalf + m_offsets[i] * 2.0;
            xpoints[i].y = m_outputs[i];
        }
        // Tell OpenGL to copy our array to the buffer object
        glBufferData(GL_ARRAY_BUFFER, sizeof xpoints, xpoints, GL_STREAM_DRAW);
        // Draw using the vertices in our vertex buffer object
        glEnableVertexAttribArray(attribute_coord2d);
        glVertexAttribPointer(attribute_coord2d, 2, GL_FLOAT, GL_FALSE, 0, 0);
		glDrawArrays(GL_POINTS, 0, WIDTH3);

        glFinish();

        uint32_t FrameBuffer[WIDTH * HEIGHT];

        const GLfloat *src = buffer;
        uint8_t *dst = (uint8_t *)FrameBuffer;

        int i, x, y;
        for (y=HEIGHT-1; y >= 0; y--) {
            for (x=0; x < WIDTH; x++) {
                int r, g, b, a;
                i = (y * WIDTH + x) * 4;
                r = (int) (src[i+0] * 255.0);
                g = (int) (src[i+1] * 255.0);
                b = (int) (src[i+2] * 255.0);
                a = (int) (src[i+3] * 255.0);
                if (b > 255) b = 255;
                if (g > 255) g = 255;
                if (r > 255) r = 255;
                if (a > 255) a = 255;
                dst[0] = (uint8_t)b;
                dst[1] = (uint8_t)g;
                dst[2] = (uint8_t)r;
                dst[3] = (uint8_t)a;
                dst += 4;
            }
        }

        Argb32VideoFrame frame_view{};
        frame_view.width_ = WIDTH;
        frame_view.height_ = HEIGHT;
        frame_view.argb32_data_ = FrameBuffer;
        frame_view.stride_ = WIDTH * 4;

        frame_request.CompleteRequest(frame_view);
        return Result::kSuccess;
    }
    
    callback2_t m_send2;

protected:
    size_t m_counter;
    int16_t m_inputs[WIDTH0];
    int16_t m_points[WIDTH2];
    float m_outputs[WIDTH3];
    float m_offsets[WIDTH3];
    rtc::scoped_refptr<webrtc::DataChannelInterface> m_data_channel;
    webrtc::PushResampler<int16_t> m_resampler;
    webrtc::AudioFrame m_inputs_frame;
    webrtc::AudioFrame m_render_frame;

    GLfloat *buffer;
    OSMesaContext ctx;

    GLuint program1;
    GLuint program2;

    GLint attribute_coord1d;
    GLint attribute_coord2d;

    GLint uniform_mytexture1;
    GLint uniform_mytexture2;

    GLuint vbo1;
    GLuint vbo2;

    std::unique_ptr<rtc::Thread> recognition_thread_;
};

#endif //WEBRTC_WRAPPER_AUDIO_SINK_H
