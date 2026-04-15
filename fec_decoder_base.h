#ifndef ___FEC_DECODRE_BASE_H___
#define ___FEC_DECODRE_BASE_H___

#include "fec_codec.h"

#include <map>
#include <vector>
#include <mutex>

struct BandFecHeaderType;
struct BandFecDecoder;

class FecDecoderBase : public IFecDecoder {
public:
    FecDecoderBase(IFecDecoderObserver* observer);

    ~FecDecoderBase() override;

    void set_max_forward_packets(int size) override;

    void set_max_packet_lifetime(const int64_t max_lifetime_ms) override;

    void set_stats_window_size(const int32_t wnd_ms) override;

    void decode(const uint8_t* data, int len) override;

    void loss_stats(PacketLossStats& stats) override;

protected:
    virtual void on_sequence_start(uint16_t sequence, const FecHeader* header, const BandFecHeaderType* bandfec_header) { }

    virtual void on_sequence_end(uint16_t sequence) { }

    virtual void on_new_block(uint16_t sequence_number, int32_t pos, const uint8_t* data, int len, bool recovered) = 0;

    int64_t get_max_packet_lifetime() const;

    void maybe_remove_outdated_decoders(int64_t now_ms, int32_t reset_sequence_number = -1);

    void decode_fec_block(uint16_t sequence_number, const BandFecHeaderType* bandfec_header, const void* data, int len, bool red);

    void send_frame(uint16_t sequence_number, uint16_t frame_number, const uint8_t* data, int data_len);

    void destroy_decoders();

    void on_block_decoded(uint16_t sequence_number, int32_t pos, const uint8_t* data, int len);

private:
    void collect_stats(uint16_t sequence);

    void remove_decoder(uint16_t sequence);

private:
    friend void on_fec_receive(BandFecDecoder* f, int64_t position, void* buf, int len, int64_t user_data1, int64_t user_data2);

    struct Stats {
        int64_t                 received_data_packets{ 0 };
        int64_t                 expected_data_packets{ 0 };
        int64_t                 recovered_packets{ 0 };
        int64_t                 missing_groups{ 0 };
        int32_t                 loss_distribution[kMaxContLossCount + 1]{ 0 };
    };

    struct Decoder {
        int32_t                 n;
        int32_t                 k;
        int64_t                 creation_time{ 0 };
        int32_t                 max_consecutive_next_group_packets{ 0 }; ///< death counter
        std::vector<int32_t>    data_packets;
        std::vector<int32_t>    red_packets;
        int32_t                 recovered_packets{ 0 };
        int64_t                 missing_groups{ 0 };
        BandFecDecoder*         decoder{ nullptr };

        void collect_stats(uint16_t sequence, Stats& stats);
    };

    enum { kMaxStatWindowMs = 3'600'000 };

    int64_t                             m_max_packet_lifetime_ms{ 3000 };
    int32_t                             m_max_forward_packets{ 3 };
    int32_t                             m_stat_window_ms{ 3000 };

    Stats                               m_global_stats;
    std::map<int64_t, Stats>            m_recent_stats;

    uint16_t                            m_latest_sequence_num{ 0 };
    IFecDecoderObserver*                m_observer;
    std::map<uint16_t, Decoder*>        m_seq_decoders;   ///< FecHeader::sequence_number

    std::mutex                          m_mutex;
};

#endif ///< ___FEC_DECODRE_BASE_H___
