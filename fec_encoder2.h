#ifndef ___FEC_ENCODER2_H___
#define ___FEC_ENCODER2_H___

#include "fec_encoder_base.h"
#include <vector>

class FecEncoder2 : public FecEncoderBase {
public:
    FecEncoder2(IFecEncoderObserver* observer);

private:
    void do_encode(const uint8_t* data, int data_len) override;

    void do_flush() override;

    FecHeader* make_fec_header(uint16_t sequence, bool red) override;

    void encode_group();

    void reset();

private:

    int                               m_max_packet_len{ 0 };
    uint16_t                          m_base_rtp_sequence_number{ 0 };
    uint32_t                          m_ssrc{ 0 };
    std::vector<std::vector<uint8_t>> m_packets;
};

#endif ///< ___FEC_ENCODER2_H___
