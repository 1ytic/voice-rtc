#include <csignal>
#include <utility>
#include <fstream>
#include <iostream>

#include <wrapper.h>

#include "App.h"

#include <rapidjson/writer.h>
#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>

#include <cereal/archives/binary.hpp>
#include <cereal/archives/json.hpp>

#include "inference/module/module.h"
#include "inference/decoder/Decoder.h"
#include "inference/module/feature/feature.h"
#include "inference/module/nn/nn.h"

#include <math.h>

using namespace w2l;
using namespace w2l::streaming;

std::shared_ptr<ConnectionWrapper> wrapper;

/* ws->getUserData returns one of these */
struct PerSocketData {
    /* Define your user data */
    int status = 1;
};

uWS::WebSocket<true, true, PerSocketData> * webSocket = nullptr;
struct uWS::Loop *loop = nullptr;
us_listen_socket_t * listenSocket = nullptr;

void my_function(int sig){
    loop->defer([webSocket, listenSocket]() {
        if (webSocket) {
            webSocket->close();
        }
        us_listen_socket_close(1, listenSocket);
    });
    wrapper->Quit();
}

void send_payload(const char *payload) {
    std::string message = std::string(payload);
    loop->defer([webSocket, message]() {
        webSocket->send(message, uWS::OpCode::TEXT);
    });
}

constexpr const float kMaxUint16 = static_cast<float>(0x8000);

float transformationFunction(int16_t i) {
    return static_cast<float>(i) / kMaxUint16;
}

std::shared_ptr<streaming::Sequential> dnnModule;
std::shared_ptr<streaming::IOBuffer> inputBuffer;
std::shared_ptr<streaming::IOBuffer> outputBuffer;

std::shared_ptr<streaming::ModuleProcessingState> input;
std::shared_ptr<streaming::ModuleProcessingState> output;

streaming::Decoder *decoderPtr;

int nSize = 8000;
int nTokens = 9998;
int nFrame = 0;

void softmax(const float* input, size_t size, float *output, size_t k = 3) {

	int i, j = 0;
	float m, sum, constant;

	m = -INFINITY;
	for (i = 0; i < size; ++i) {
		if (m < input[i]) {
			m = input[i];
            j = i;
		}
	}

	sum = 0.0;
	for (i = 0; i < size; ++i) {
		sum += (float)std::exp(input[i] - m);
	}

	constant = m + std::log(sum);
    
    std::vector<size_t> idx(size);

    std::iota(idx.begin(), idx.end(), 0);

    std::partial_sort(
        idx.begin(),
        idx.begin() + k,
        idx.end(),
        [&input](const size_t& l, const size_t& r) {
            return input[l] > input[r];
        }
    );

    *output = 0;

	for (i = 0; i < k; ++i) {
        j = idx[i];
        if (j != 9997) {
		    float p = (float)std::exp(input[j] - constant);
            if (i == 0) {
                *output = p;
            }
            if (p > 0.01) {
                std::cout << "frame: " << nFrame << "/" << j << " (" << p << ")" << std::endl;
            }
        }
	}
    
    nFrame += 1;
}

void send_audio_data(const int16_t* audio_data, size_t data_size, float *outputs) {
    if (data_size != nSize) {
        return;
    }
    inputBuffer->ensure<float>(data_size);
    //std::cout << "buffer allocated" << std::endl;
    float* inputBufferPtr = inputBuffer->data<float>();
    std::transform(audio_data, audio_data + data_size, inputBufferPtr, transformationFunction);
    //std::cout << "data transformed" << std::endl;
    inputBuffer->move<float>(data_size);
    //std::cout << "audio data copied" << std::endl;
    dnnModule->run(input);
    //std::cout << "dnn module finished" << std::endl;
    float* data = outputBuffer->data<float>();
    int size = outputBuffer->size<float>();
    //std::cout << "output buffer: " << size << std::endl;
    if (data && size > 0) {
        decoderPtr->run(data, size);
        //std::cout << "decoder finished" << std::endl;
    }
    constexpr const int lookBack = 0;
    const std::vector<WordUnit>& words = decoderPtr->getBestHypothesisInWords(lookBack);
    if (words.size() > 0) {
        for (const auto& word : words) {
            std::cout << "word: " << word.word << std::endl;
        }
    }
    
    const int nFramesOut = size / nTokens;

    for (int i = 0; i < nFramesOut; i++) {
        softmax(data, nTokens, &outputs[i]);
        data += nTokens;
    }

    // Consume and prune
    outputBuffer->consume<float>(nFramesOut * nTokens);
    decoderPtr->prune(lookBack);
}

int main() {

    signal(SIGINT, my_function);

    std::string modelsPath = "/home/ubuntu/wav2letter/models/";
    std::string featurePath = "feature_extractor.bin";
    std::string acousticPath = "acoustic_model.bin";
    std::string tokensPath = "tokens.txt";
    std::string optionsPath = "decoder_options.json";
    std::string lexiconPath = "lexicon.txt";
    std::string languagePath = "language_model.bin";

    std::shared_ptr<streaming::Sequential> featureModule;
    std::shared_ptr<streaming::Sequential> acousticModule;

    {
        std::ifstream featureFile(modelsPath + featurePath, std::ios::binary);
        if (!featureFile.is_open()) {
            throw std::runtime_error("failed to open " + featurePath);
        }
        cereal::BinaryInputArchive featureArchive(featureFile);
        featureArchive(featureModule);
    }

    {
        std::ifstream acousticFile(modelsPath + acousticPath, std::ios::binary);
        if (!acousticFile.is_open()) {
            throw std::runtime_error("failed to open " + acousticPath);
        }
        cereal::BinaryInputArchive acousticArchive(acousticFile);
        acousticArchive(acousticModule);
    }

    // String both models togethers to a single DNN.
    dnnModule = std::make_shared<streaming::Sequential>();
    dnnModule->add(featureModule);
    dnnModule->add(acousticModule);

    //std::cout << dnnModule->debugString() << std::endl;

    //std::ifstream transitionsFile(modelsPath + transitionsPath, std::ios::binary);
    //if (!transitionsFile.is_open()) {
    //  throw std::runtime_error("failed to open " + transitionsPath);
    //}
    //cereal::BinaryInputArchive transitionsArchive(transitionsFile);
    //transitionsArchive(transitions);
    std::vector<float> transitions;

    std::shared_ptr<const DecoderFactory> decoderFactory = std::make_shared<DecoderFactory>(
        modelsPath + tokensPath,
        modelsPath + lexiconPath,
        modelsPath + languagePath,
        transitions,
        fl::lib::text::SmearingMode::MAX,
        "_",
        0
    );

    fl::lib::text::LexiconDecoderOptions decoderOptions;

    {
        std::ifstream optionsFile(modelsPath + optionsPath);
        if (!optionsFile.is_open()) {
            throw std::runtime_error("failed to open " + optionsPath);
        }
        cereal::JSONInputArchive optionsJson(optionsFile);
        optionsJson(
            cereal::make_nvp("beamSize", decoderOptions.beamSize),
            cereal::make_nvp("beamSizeToken", decoderOptions.beamSizeToken),
            cereal::make_nvp("beamThreshold", decoderOptions.beamThreshold),
            cereal::make_nvp("lmWeight", decoderOptions.lmWeight),
            cereal::make_nvp("wordScore", decoderOptions.wordScore),
            cereal::make_nvp("unkScore", decoderOptions.unkScore),
            cereal::make_nvp("silScore", decoderOptions.silScore)//,
            //cereal::make_nvp("logAdd", decoderOptions.logAdd),
            //cereal::make_nvp("criterionType", decoderOptions.criterionType)
        );
        decoderOptions.logAdd = false;
        decoderOptions.criterionType = fl::lib::text::CriterionType::CTC;
    }

    auto decoder = decoderFactory->createDecoder(decoderOptions);

    std::cout << "Decoder created" << std::endl;

    decoder.start();

    decoderPtr = &decoder;

    std::cout << "Decoder started" << std::endl;

    input = std::make_shared<streaming::ModuleProcessingState>(1);
    output = dnnModule->start(input);

    inputBuffer = input->buffer(0);
    outputBuffer = output->buffer(0);

    wrapper = std::make_shared<ConnectionWrapper>();
    //wrapper->Join();

    loop = uWS::Loop::get();

	auto app = uWS::SSLApp({
        .key_file_name = "/home/ubuntu/uWebSockets/misc/key.pem",
        .cert_file_name = "/home/ubuntu/uWebSockets/misc/cert.pem",
        .passphrase = "1234"
    }).get("/", [](auto *res, auto *req) {

        std::ifstream file("html/client.html");
        std::stringstream stream;
        stream << file.rdbuf();
        res->end(stream.str());
        file.close();

	}).get("/client.js", [](auto *res, auto *req) {

        std::ifstream file("html/client.js");
        std::stringstream stream;
        stream << file.rdbuf();
        res->end(stream.str());
        file.close();

	}).get("/client.css", [](auto *res, auto *req) {

        std::ifstream file("html/client.css");
        std::stringstream stream;
        stream << file.rdbuf();
        res->end(stream.str());
        file.close();

	}).ws<PerSocketData>("/*", {
        /* Handlers */
        .open = [](auto *ws) {
            webSocket = ws;
        },
        .message = [](auto *ws, std::string_view message, uWS::OpCode opCode) {
            rapidjson::Document json;
            json.Parse(std::string(message).c_str());
            if (json.HasMember("type")) {
                std::string type = json["type"].GetString();
                if (type == "offer") {
                    if (json.HasMember("payload")) {
                        const rapidjson::Value& payload = json["payload"];
                        if (payload.IsObject()) {
                            std::string sdp = payload["sdp"].GetString();
                            std::cout << "SDP: " << sdp << std::endl;
                            wrapper->CreateConnection(sdp.c_str(), send_payload, send_audio_data);
                        }
                    }
                } else if (type == "candidate") {
                    if (json.HasMember("payload")) {
                        const rapidjson::Value& payload = json["payload"];
                        if (payload.IsObject()) {
                            std::string sdp_mid = payload["sdpMid"].GetString();
                            int sdp_mline_index = payload["sdpMLineIndex"].GetInt();
                            std::string candidate = payload["candidate"].GetString();
                            wrapper->AddCandidate(sdp_mid.c_str(), sdp_mline_index, candidate.c_str());
                        }
                    }
                }
            }

        },
        .close = [](auto */*ws*/, int /*code*/, std::string_view /*message*/) {
            /* You may access ws->getUserData() here, but sending or
             * doing any kind of I/O with the socket is not valid. */
        }
    }).listen(8080, [](auto *socket) {
	    if (socket) {
			std::cout << "Listening on port " << 8080 << std::endl;
	        listenSocket = socket;
	    }
	});

    app.run();

    std::cout << "Gracefully shutdown" << std::endl;

    return 0;
}
