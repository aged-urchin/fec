#include "network_conditioner.h"
#include "fec_codec.h"

#include <iostream>
#include <map>
#include <vector>
#include <string>
#include <fstream>
#include <windows.h>

#define USE_RANDOM_FILE false
RandomLossTool traffic(LossRateType::LOSS_30_PERCENT);

const int kFecParamS = 1024;
const int kFecParamN = 10;
const int kFecParamK = 12;

FILE* random_in = nullptr, *recv_block = nullptr;
std::vector<int32_t> random_numbers_in;

class Foo : public IFecEncoderObserver,
            public IFecDecoderObserver {
public:
    void start(const char* name) {
        m_out_file = fopen(name, "wb");

        random_in  = fopen("random_in.txt", "w");
        recv_block = fopen("recv_block.txt", "w");
#if USE_RANDOM_FILE
        std::ifstream file("read_random.txt");
        std::string line;

        while (std::getline(file, line)) {
            random_numbers_in.push_back(std::stoi(line));
        }
#endif

        m_encoder = create_fec_encoder(this);
        m_decoder = create_fec_decoder(this);

        m_encoder->set_param(kFecParamS, kFecParamN, kFecParamK);
    }

    void stop() {
        if (m_encoder) {
            m_encoder->flush();

            destroy_fec_encoder(m_encoder);
            m_encoder = nullptr;
        }

        destroy_fec_decoder(m_decoder);
        m_decoder = nullptr;

        int constructed_frames = 0;
        for (auto& sequence_frames : m_frames) {
            for (auto& frame : sequence_frames.second.frames) {
                ++constructed_frames;
                fwrite(frame.second.data(), 1, frame.second.size(), m_out_file);
            }
        }
        fclose(m_out_file);
        std::cerr << "frame lossrate: " << (m_encoded_frames - constructed_frames) * 100.0 / m_encoded_frames << "%" << std::endl;
    }

    void push_data(const std::vector<uint8_t>& data) {
        m_encoder->encode(data.data(), data.size());
        ++m_encoded_frames;
    }

    void update(int N, int K) {
        m_encoder->set_param(kFecParamS, N, K);
    }

private:
    void on_encoder_output(IFecEncoder* encoder, IFecPacket* packet) override {
        static int out_packets = 0;
        ++out_packets;
#if USE_RANDOM_FILE
        if (random_numbers_in.end() != std::find(random_numbers_in.begin(), random_numbers_in.end(), out_packets)) {
#else
		if (!traffic.is_packet_lost()) {
#endif
            m_decoder->decode((uint8_t*)packet->get_buffer(), packet->get_buffer_size());
        }
    }

    void on_decoder_output(IFecDecoder* decoder, uint16_t sequence_number, uint16_t frame_number, const uint8_t* data, int len) override {
        m_frames[sequence_number].frames[frame_number] = { data, data + len };
    }

private:

    struct SequenceData {
        std::map<uint16_t, std::vector<uint8_t>> frames;
    };

    bool                                    m_new_line{ false };
    IFecEncoder*                            m_encoder{ nullptr };
    IFecDecoder*                            m_decoder{ nullptr };
    FILE*                                   m_out_file{ nullptr };
    uint16_t                                m_next_frame{ 0 };
    std::map<uint16_t, SequenceData>        m_frames;
    int                                     m_encoded_frames{ 0 };
};

int main() {
    system("chcp 65001");

    const int kReadSize = 1000;
    std::vector<uint8_t> data(kReadSize);

    auto f = fopen("camera.264", "rb");
    fseek(f, 0, SEEK_END);
    auto file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    int in_packets = 0, in_size = 0, progress = -1;

    Foo foo;
    foo.start("recovered.264");
    do {
        if (fread(data.data(), 1, kReadSize, f) < kReadSize) {
            break;
        }

        foo.push_data(data);
        in_size += kReadSize;
#if 0
        if (in_size / kReadSize % 500 == 0) {
            static int rnd[] = { 5, 10, 15, 20 };
            static int idx = 0;

            auto p = rnd[++idx % (sizeof(rnd) / sizeof(rnd[0]))];
            foo.update(p, p);
        }
#endif
        auto new_progress = (int)(in_size * 100.f / file_size);
        if (new_progress >= progress + 1) {
            SetConsoleTitleA((std::to_string(new_progress) + "%").c_str());
            progress = new_progress;
        }
    } while (true);

    foo.stop();
    fclose(f);
}
