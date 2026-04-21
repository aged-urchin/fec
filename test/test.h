#ifndef ___TEST_H___
#define ___TEST_H___

#include "network_conditioner.h"
#include "fec_data_checker.h"
#include "../fec_codec.h"

#include <cassert>
#include <iostream>
#include <map>
#include <set>
#include <vector>
#include <string>
#include <iomanip>
#include <numeric>
#include <sstream>
#include <windows.h>

namespace TEST {

    RandomLossTool traffic(LossRateType::LOSS_30_PERCENT);
    FecChecker data_checker;

    const int kFecParamS = 1024;
    const int kFecParamN = 10;
    const int kFecParamK = 10;

    class Foo : public IFecEncoderObserver,
        public IFecDecoderObserver {
    public:
        void start() {
            m_encoder = create_fec_encoder(kFecTypeBand, kFecModeCompact, this);
            m_decoder = create_fec_decoder(this);
            //m_decoder->set_max_packet_lifetime(INT64_MAX);

            m_encoder->set_block_size(kFecParamS);
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
            destroy_fec_decoder(m_decoder, &stats);
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
                   "total frames: %d, , recv frames: %d, frame lossrate: %f%%\n",
                   m_encoded_frames, m_constructed_frames, (m_encoded_frames - m_constructed_frames) * 100.0 / m_encoded_frames);
        }

        void push_data(const uint8_t* data, int len) {
            m_encoder->encode(data, len);
            ++m_encoded_frames;
        }

        void update(int N, int K) {
            m_encoder->set_red_params(N, K);
        }

    private:
        int64_t get_current_ms() {
            auto now = std::chrono::system_clock::now();
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

            return ms;
        }

        void on_encoder_output(IFecEncoder* encoder, IFecPacket* packet) override {
            if (!traffic.is_packet_lost()) {
                m_decoder->decode((uint8_t*)packet->get_packet_buffer(), packet->get_packet_buffer_size());
            }
        }

        void on_decoder_output(IFecDecoder* decoder, uint16_t sequence_number, uint16_t frame_number, const uint8_t* data, int len) override {
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

            ++m_constructed_frames;
        }

private:

    int64_t                                 m_t0;
    IFecEncoder* 							m_encoder{ nullptr };
    IFecDecoder* 							m_decoder{ nullptr };
    FILE* 									m_out_file{ nullptr };
    uint16_t                                m_next_frame{ 0 };
    int32_t                                 m_encoded_frames{ 0 };
    int32_t                                 m_constructed_frames{ 0 };
    std::map<uint16_t, std::set<uint16_t>>  m_recv_packets;
};

void test() {
    const int kTotalPackets = 100000;
    int in_packets = 0, progress = -1;

    Foo foo;
    foo.start();

    std::vector<uint8_t> user_data(1500); ///< less than MTU
    for (int i = 0; i < kTotalPackets; ++i) {
        uint32_t out_len = 0;
        if (!data_checker.generate(user_data.data(), 1300, out_len)) {
            assert(0);
        }

        foo.push_data(user_data.data(), out_len);
        ++in_packets;

        auto new_progress = (int)(in_packets * 100.f / kTotalPackets);
        if (new_progress >= progress + 1) {
            SetConsoleTitleA((std::to_string(new_progress) + "%").c_str());
            progress = new_progress;
        }
    }

    foo.stop();
}

}
#endif ///< ___TEST_H___
