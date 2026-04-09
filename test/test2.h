#ifndef ___TEST2_H___
#define ___TEST2_H___

#include "./network_conditioner.h"
#include "../fec_codec.h"
#include "../utils/utils.h"

#include <iostream>
#include <map>
#include <vector>
#include <string>
#include <fstream>
#include <windows.h>

namespace TEST2 {

RandomLossTool traffic(LossRateType::LOSS_30_PERCENT);

const int kFecParamN = 10;
const int kFecParamK = 12;

class Foo : public IFecEncoderObserver,
            public IFecDecoderObserver {
public:
    void start(const char* name) {
        m_out_file = fopen(name, "wb");

        m_encoder = create_fec_encoder2(this);
        m_decoder = create_fec_decoder2(this);

        m_encoder->set_red_params(kFecParamN, kFecParamK);
    }

    void stop() {
        if (m_encoder) {
            m_encoder->flush();

            destroy_fec_encoder2(m_encoder);
            m_encoder = nullptr;
        }

        PacketLossStats stats;
        destroy_fec_decoder2(m_decoder, stats);
        m_decoder = nullptr;

        std::cerr << "stats -- packet lossrate: " << (stats.lossrate * 100)
            << "%, effective packet lossrate: " << (stats.effective_lossrate * 100) << "%, missing groups: " << stats.missing_groups << std::endl;
#if 0
        int constructed_frames = 0;
        for (auto& sequence_frames : m_frames) {
            for (auto& frame : sequence_frames.second.frames) {
                ++constructed_frames;
                fwrite(frame.second.data(), 1, frame.second.size(), m_out_file);
            }
        }
        fclose(m_out_file);
#endif
        std::cerr << "+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++" << std::endl;
        std::cerr << "frame lossrate before fec: " << m_total_losts * 100.0 / m_total_packets << "%" << std::endl;
        std::cerr << "red packets lossrate: " << m_lost_red_packetes * 100.0 / m_total_red_packets << "%" << std::endl;
        std::cerr << "rtp packets lossrate before fec: " << m_lost_rtp_packets * 100.0 / m_total_rtp_packets << "%" << std::endl;
        std::cerr << "rtp packets lossrate after fec: " << (m_lost_rtp_packets - m_recovered_rtp_packets) * 100.0 / m_lost_rtp_packets << "%" << std::endl;
    }

    void push_data(const std::vector<uint8_t>& data) {
        decode(data.data(), (int)data.size(), true);
        ++m_total_rtp_packets;

        m_encoder->encode(data.data(), (int)data.size());
        ++m_encoded_frames;
    }

    void update(int N, int K) {
        m_encoder->set_red_params(N, K);
    }

private:
    void decode(const uint8_t* data, int len, bool rtp) {
        ++m_total_packets;

        if (!traffic.is_packet_lost()) {
            m_decoder->decode(data, len);
        } else {
            ++m_total_losts;

            if (rtp) {
                RtpHeader rtp_header;
                if (!parse_rtp_buffer(data, len, rtp_header)) {
                    std::cerr << "input an invalid rtp packet" << std::endl;
                    return;
                }

                ++m_lost_rtp_packets;
                std::cerr << "rtp lost seq: " << rtp_header.seq << std::endl;
            } else {
                ++m_lost_red_packetes;
            }
        }
    }

    void on_encoder_output(IFecEncoder* encoder, IFecPacket* packet) override {
        auto header = packet->get_header();
        assert(header->red);

        ++m_total_red_packets;
        decode((uint8_t*)packet->get_packet_buffer(), packet->get_packet_buffer_size(), false);
    }

    void on_decoder_output(IFecDecoder* decoder, uint16_t sequence_number, uint16_t frame_number, const uint8_t* data, int len) override {
        m_frames[sequence_number].frames[frame_number] = { data, data + len };

        RtpHeader rtp_header;
        if (!parse_rtp_buffer(data, len, rtp_header)) {
            std::cerr << "output an invalid rtp packet" << std::endl;
            return;
        }

        ++m_recovered_rtp_packets;
        std::cerr << "rtp recovered seq: " << rtp_header.seq << std::endl;
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

    int                                     m_total_packets{ 0 };
    int                                     m_total_losts{ 0 };

    int                                     m_total_red_packets{ 0 };
    int                                     m_lost_red_packetes{ 0 };

    int                                     m_total_rtp_packets{ 0 };
    int                                     m_lost_rtp_packets{ 0 };
    int                                     m_recovered_rtp_packets{ 0 };
};

void test() {
    auto f = fopen("./test/rtp_packets.bin", "rb");
    fseek(f, 0, SEEK_END);
    auto file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    int in_packets = 0, in_size = 0, progress = -1;

    Foo foo;
    foo.start("./test/recovered.rtp");

    do {
        uint16_t pkt_len = 0;
        if (fread(&pkt_len, 1, 2, f) != 2) {
            break;
        }

        std::vector<uint8_t> data;
        data.resize(pkt_len);

        if (fread(data.data(), 1, pkt_len, f) < pkt_len) {
            break;
        }

        foo.push_data(data);
        in_size += (pkt_len + 2);
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

    fclose(f);
    foo.stop();
}

}
#endif ///< ___TEST2_H___
