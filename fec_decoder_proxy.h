#ifndef ___FEC_DECODER_PROXY_H___
#define ___FEC_DECODER_PROXY_H___

#include "fec_codec.h"
#include <vector>

class FecDecoderProxy : public IFecDecoder {
public:
    explicit FecDecoderProxy(IFecDecoderObserver* observer);

    ~FecDecoderProxy() override;

    void destroy(PacketLossStats* stats);

    void set_max_forward_packets(int packets) override;

    void set_max_packet_lifetime(const int64_t max_lifetime_ms) override;

    void set_stats_window_size(const int32_t wnd_ms) override;

    void decode(const uint8_t* data, int len) override;

    void loss_stats(PacketLossStats& stats) override;

    FecMode mode() override;

private:
    void destroy_real_decoder();

    void rebuild_decoder(FecType new_type, FecMode new_mode);

private:

    IFecDecoderObserver*    m_observer{ nullptr };
    FecType                 m_current_type{ kFecTypeBand };
    IFecDecoder*            m_decoder{ nullptr };

    int                     m_cached_max_forward_packets{ 0 };
    bool                    m_cached_max_forward_packets_set{ false };
    int64_t                 m_cached_max_lifetime_ms{ 0 };
    bool                    m_cached_max_lifetime_set{ false };
    int32_t                 m_cached_stats_wnd_ms{ 0 };
    bool                    m_cached_stats_wnd_set{ false };

    std::vector<std::vector<uint8_t>> m_buffered_blocks;
};

#endif ///< ___FEC_DECODER_PROXY_H___
