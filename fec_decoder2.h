#ifndef ___FEC_DECODER2_H___
#define ___FEC_DECODER2_H___

#include "fec_decoder_base.h"
#include <vector>

class FecDecoder2 : public FecDecoderBase {
public:
    FecDecoder2(FecType type, IFecDecoderObserver* observer);

    ~FecDecoder2() override;

    void destroy(PacketLossStats* stats);

    void decode(const uint8_t* data, int len) override;

private:
    struct FecGroup {
        uint16_t                        sequence;
        FecHeaderInfo                   header_info;
        std::map<uint16_t, uint16_t>    rtp_packet_sizes;

        FecGroup(uint16_t sequence, const FecHeaderInfo* info, SoftRtp* ext);
        ~FecGroup();
    };

    bool is_rtp(const uint8_t* data, size_t len);

    void purge_expired_data(int64_t now_ms);

    bool maybe_decode_rtp_packet(uint16_t rtp_sequence, const uint8_t* rtp_packet_data, int rtp_packet_len);

    void decode_rtp(uint16_t rtp_sequence, const FecGroup* fec_group, const void* data, int len);

    void on_sequence_start(uint16_t sequence, const FecHeader* header, const FecHeaderInfo* header_info) override;

    void on_sequence_end(uint16_t sequence) override;

    void on_new_block(uint16_t sequence_number, int32_t pos, const uint8_t* data, int len, bool recovered) override;

private:

    struct StoredRtpPacket{
        int64_t              arrivetime_ms;
        std::vector<uint8_t> packet;
    };

    int64_t                                 m_ssrc{ -1 };
    std::map<uint16_t, StoredRtpPacket>     m_rtp_packets;
    std::map<uint16_t, FecGroup*>           m_fec_groups;
};

#endif ///< ___FEC_DECODER2_H___
