#ifndef ___FEC_DECODRE_BASE_H___
#define ___FEC_DECODRE_BASE_H___

#include "fec_codec.h"
#include "fec_adapter.h"

#include <map>
#include <vector>
#include <mutex>

class FecDecoderBase : public IFecDecoder,
                       public IFecDecoderAdapterObserver {
public:
    FecDecoderBase(FecType type, FecMode mode, IFecDecoderObserver* observer);

    ~FecDecoderBase() override;

    void set_max_forward_packets(int size) override;

    void set_max_packet_lifetime(const int64_t max_lifetime_ms) override;

    void set_stats_window_size(const int32_t wnd_ms) override;

    void decode(const uint8_t* data, int len) override;

    void loss_stats(PacketLossStats& stats) override;

    FecMode mode() override { return m_mode; }

protected:
    virtual void on_sequence_start(uint16_t sequence, const FecHeader* header, const FecHeaderInfo* header_info) { }

    virtual void on_sequence_end(uint16_t sequence) { }

    virtual void on_new_block(uint16_t sequence_number, int32_t pos, const uint8_t* data, int len, bool recovered) = 0;

    int64_t get_max_packet_lifetime() const;

    void maybe_remove_outdated_decoders(int64_t now_ms, int32_t reset_sequence_number = -1);

    void decode_fec_block(uint16_t sequence_number, const FecHeaderInfo* header_info, const void* data, int len, bool red);

    void send_frame(uint16_t sequence_number, uint16_t frame_number, const uint8_t* data, int data_len);

    void destroy_decoders(PacketLossStats* stats);

private:
    void on_decoder_output(IFecDecoderAdapter* adapter, uint16_t sequence_number, int32_t pos, const uint8_t* data, int32_t len) override;

    FecHeaderInfo get_header_info(const void* block, int32_t size);

    IFecDecoderAdapter* create_decoder(uint16_t sequence);

    void collect_stats(uint16_t sequence);

    void remove_decoder(uint16_t sequence);

    struct Stats {
        int64_t                 received_data_packets{ 0 };
        int64_t                 expected_data_packets{ 0 };
        int64_t                 recovered_packets{ 0 };
        int64_t                 missing_groups{ 0 };
        int64_t                 loss_distribution[kMaxContLossCount + 1]{ 0 };
    };

    void convert_stats(PacketLossStats* packet_stats, const Stats& stats);

private:

    struct Decoder {
        int32_t                 n;
        int32_t                 k;
        int64_t                 creation_time{ 0 };
        int32_t                 max_consecutive_next_group_packets{ 0 }; ///< death counter
        std::vector<int32_t>    data_packets;
        std::vector<int32_t>    red_packets;
        int32_t                 recovered_packets{ 0 };
        int64_t                 missing_groups{ 0 };
        IFecDecoderAdapter*     decoder{ nullptr };

        void collect_stats(uint16_t sequence, Stats& stats, FecType type);
    };

    enum { kMaxStatWindowMs = 3'600'000 };

    const FecType                       m_type;
    const FecMode                       m_mode;
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
