#ifndef ___TEST2_H___
#define ___TEST2_H___

#include "network_conditioner.h"
#include "../fec_codec.h"
#include "../utils/utils.h"
#include "fec_data_checker.h"

#include <iostream>
#include <map>
#include <vector>
#include <string>
#include <iomanip>
#include <numeric>
#include <sstream>
#include <windows.h>

namespace TEST2 {

#define TEST_DECODER_API2 true

RandomLossTool traffic(LossRateType::LOSS_30_PERCENT);
FecChecker data_checker;

const FecType kFecType = kFecTypeBand;
const int kFecParamN = 10;
const int kFecParamK = 10;

class Foo : public IFecEncoderObserver,
            public IFecDecoderObserver {
public:
    void start() {
        printf("=======================TEST2========================\n");
        m_encoder = create_fec_encoder(kFecType, kFecModeSoftRtp, this);
#if TEST_DECODER_API2
        m_decoder = create_fec_decoder2(kFecType, kFecModeSoftRtp, this);
#else
        m_decoder = create_fec_decoder(this);
#endif
        m_encoder->set_red_params(kFecParamN, kFecParamK);
        m_t0 = get_current_ms();
    }

    void stop() {
        if (m_encoder) {
            m_encoder->flush();

            destroy_fec_encoder(m_encoder);
            m_encoder = nullptr;
        }

        PacketLossStats stats;
#if TEST_DECODER_API2
        destroy_fec_decoder2(m_decoder, &stats);
#else
        destroy_fec_decoder(m_decoder, &stats);
#endif
        m_decoder = nullptr;

        printf("stats -- packet lossrate: %f%% , effective packet lossrate: %f%%, missing groups: %lld\n",
                (stats.lossrate * 100), (stats.effective_lossrate * 100), stats.missing_groups);

        auto total_losses = std::accumulate(stats.loss_dist, stats.loss_dist + std::size(stats.loss_dist), 0LL);
        std::ostringstream os;
        for (int i = 0; i < sizeof(stats.loss_dist) / sizeof(stats.loss_dist[0]); ++i) {
            if (stats.loss_dist[i] > 0) {
                os << std::setw(2) << i << ": " << stats.loss_dist[i] << " (" << std::setprecision(2) << stats.loss_dist[i] * 100. / total_losses << "%)" << std::endl;
            }
        }
        printf("time cost: %lld(ms)\n", get_current_ms() - m_t0);
        printf("loss distributions: \n%s\n", os.str().c_str());
        printf("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n"
               "lossrate before fec: %f%%(rtp: %f%%, red: %f%%)\n"
               "rtp packets lossrate after fec: %f%%\n",
                m_total_losts * 100.0 / m_total_packets,
                m_lost_rtp_packets * 100.0 / m_total_rtp_packets,
                m_lost_red_packetes * 100.0 / m_total_red_packets,
                (m_lost_rtp_packets - m_recovered_rtp_packets) * 100.0 / m_lost_rtp_packets);
    }

    void push_data(const std::vector<uint8_t>& data) {
        std::vector<uint8_t> crc_data(data.size() + 8);
        data_checker.add_check(data.data(), data.size(), crc_data.data(), crc_data.size());

        decode(crc_data.data(), crc_data.size(), true);
        ++m_total_rtp_packets;

        m_encoder->encode(crc_data.data(), crc_data.size());
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
        RtpHeader rtp_header;
        if (!parse_rtp_buffer(data, len, rtp_header)) {
            std::cerr << "output an invalid rtp packet" << std::endl;
            return;
        }

        std::cerr << "rtp recovered seq: " << rtp_header.seq << std::endl;

        if (!data_checker.check(data, len)) {
            assert(0);
            printf("data check failed\n");
            return;
        }

        auto ret = m_recv_packets[sequence_number].insert(frame_number);
        if (!ret.second) {
            assert(0);
            printf("receive duplicat packet\n");
            return;
        }

        ++m_recovered_rtp_packets;
    }

    int64_t get_current_ms() {
        auto now = std::chrono::system_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

        return ms;
    }

private:

    IFecEncoder*                            m_encoder{ nullptr };
    IFecDecoder*                            m_decoder{ nullptr };
    uint16_t                                m_next_frame{ 0 };

    int                                     m_encoded_frames{ 0 };

    int                                     m_total_packets{ 0 };
    int                                     m_total_losts{ 0 };

    int                                     m_total_red_packets{ 0 };
    int                                     m_lost_red_packetes{ 0 };

    int                                     m_total_rtp_packets{ 0 };
    int                                     m_lost_rtp_packets{ 0 };
    int                                     m_recovered_rtp_packets{ 0 };
    std::map<uint16_t, std::set<uint16_t>>  m_recv_packets;

    int64_t                                 m_t0;
};

void test() {
    auto f = fopen("./test/rtp_packets.bin", "rb");
    fseek(f, 0, SEEK_END);
    auto file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    int in_packets = 0, in_size = 0, progress = -1;

    Foo foo;
    foo.start();

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
