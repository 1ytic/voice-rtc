let webSocketConnection = null;
let rtcPeerConnection = null;
let dataChannel = null;

function onDataChannelMessage(event) {
  console.log(event.data);
}

function onDataChannelOpen() {
  console.log("Data channel opened!");
}

function onIceCandidate(event) {
  if (event && event.candidate) {
    webSocketConnection.send(JSON.stringify({type: "candidate", payload: event.candidate}));
  }
}

function handleNegotiationNeededEvent() {
  const sdpConstraints = {};
  rtcPeerConnection.createOffer(sdpConstraints)
    .then((offer) => rtcPeerConnection.setLocalDescription(offer))
    .then(() => {
      webSocketConnection.send(JSON.stringify({type: "offer", payload: rtcPeerConnection.localDescription}));
    })
    .catch(reportError);
}

function handleTrackEvent(event) {
  video = document.getElementById("video");
  video.srcObject = new MediaStream([event.transceiver.receiver.track]);
}

function handleConnectionStateChange(event) {
  button = document.getElementById("button");
  video = document.getElementById("video");
  if (rtcPeerConnection.connectionState == "disconnected") {
    button.style.display = "block";
    video.style.display = "none";
    button.disabled = false;
  } else if (rtcPeerConnection.connectionState == "connected") {
    button.style.display = "none";
    video.style.display = "block";
  }
}

function onWebSocketClose() {
  button = document.getElementById("button");
  video = document.getElementById("video");
  button.style.display = "block";
  video.style.display = "none";
  button.disabled = false;
}

function onWebSocketOpen() {
  const config = {iceServers: [{urls: ["stun:stun.l.google.com:19302"]}], sdpSemantics: "unified-plan"};
  rtcPeerConnection = new RTCPeerConnection(config);
  rtcPeerConnection.onicecandidate = onIceCandidate;
  rtcPeerConnection.ontrack = handleTrackEvent;
  rtcPeerConnection.onnegotiationneeded = handleNegotiationNeededEvent;
  rtcPeerConnection.onconnectionstatechange = handleConnectionStateChange;
  const mediaConstraints = {audio: true, video: false}
  navigator.mediaDevices.getUserMedia(mediaConstraints)
  .then((stream) => {
    console.log("addTransceiver: audio");
    const taudio = rtcPeerConnection.addTransceiver(stream.getAudioTracks()[0], {streams: [stream]});
    console.log(taudio);
    console.log("addTransceiver: video");
    const tvideo = rtcPeerConnection.addTransceiver("video", {direction: "recvonly", streams: [stream]});
    console.log(tvideo);
    let vp9_codecs = [];
    let codecs = RTCRtpReceiver.getCapabilities("video").codecs;
    // iterate over supported codecs and pull out the codecs we want
    for (let i = 0; i < codecs.length; i++) {
      if (codecs[i].mimeType == "video/VP9") {
        vp9_codecs.push(codecs[i]);
      }
      console.log(codecs[i].mimeType);
    }
    // currently not all browsers support setCodecPreferences
    if (tvideo.setCodecPreferences != undefined) {
      tvideo.setCodecPreferences(vp9_codecs);
    }
  }).then(() => {
    const dataChannelConfig = {ordered: false, maxRetransmits: 0};
    dataChannel = rtcPeerConnection.createDataChannel("dc", dataChannelConfig);
    dataChannel.onmessage = onDataChannelMessage;
    dataChannel.onopen = onDataChannelOpen;
  })
}

function onWebSocketMessage(event) {
  const message = JSON.parse(event.data);
  if (message.type === "answer") {
    const desc = new RTCSessionDescription(message.payload);
    rtcPeerConnection.setRemoteDescription(desc);
  } else if (message.type === "candidate") {
    rtcPeerConnection.addIceCandidate(new RTCIceCandidate(message.payload));
  }
}

function connect() {
  document.getElementById("button").disabled = true;
  webSocketConnection = new WebSocket("wss://" + window.location.host);
  webSocketConnection.onopen = onWebSocketOpen;
  webSocketConnection.onclose = onWebSocketClose;
  webSocketConnection.onmessage = onWebSocketMessage;
}
