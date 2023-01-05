cmake_minimum_required(VERSION 3.21)
project(wrapper VERSION 1.0.1 DESCRIPTION "WebRTC shared library wrapper")

include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 17)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/")

add_definitions(-DWEBRTC_POSIX -DWEBRTC_LINUX -DNDEBUG -DGL_GLEXT_PROTOTYPES)

include_directories("${VANILLA_WEBRTC_SRC}/webrtc")
include_directories("/home/ubuntu/rapidjson/include")

find_package(Threads REQUIRED)
find_package(LibWebRTC REQUIRED)

add_library(wrapper SHARED
        wrapper.cpp
        audio_sink.h
        peer_connection_observer.h
        data_channel_observer.h
        create_session_observer.h
        set_session_observer.h
        peer_connection_observer.cpp
        data_channel_observer.cpp
        create_session_observer.cpp
        video/track_source.cpp
        video/shader_utils.cpp
        ${VANILLA_WEBRTC_SRC}/webrtc/pc/video_track_source.cc
        ${VANILLA_WEBRTC_SRC}/webrtc/api/video/video_frame_buffer.cc
        ${VANILLA_WEBRTC_SRC}/webrtc/media/base/adapted_video_track_source.cc
)

set_target_properties(
        wrapper PROPERTIES
        VERSION ${PROJECT_VERSION}
        SOVERSION 1
        PUBLIC_HEADER wrapper.h
)

target_link_libraries(wrapper pthread LibWebRTC::LibWebRTC)

install(TARGETS wrapper
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
