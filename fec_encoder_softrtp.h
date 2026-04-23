#ifndef ___FEC_ENCODER_SOFTRTP_H___
#define ___FEC_ENCODER_SOFTRTP_H___

#include "fec_encoder_base.h"
#include <vector>

class FecEncoderSoftRtp : public FecEncoderBase {
public:
    FecEncoderSoftRtp(FecType type, IFecEncoderObserver* observer);

    bool set_red_params(int blocks_in_group, int red_blocks_in_group) override;

private:
    void do_encode(const uint8_t* data, int data_len) override;

    void do_flush() override;

    FecHeader* make_fec_header(uint16_t sequence, bool red) override;

    void encode_group();

    void reset();

private:

    int                                 m_max_packet_len{ 0 };
    uint16_t                            m_base_rtp_sequence_number{ 0 };
    uint32_t                            m_ssrc{ 0 };
    std::vector<uint8_t>                m_delta_sizes;
    std::vector<std::vector<uint8_t>>   m_packets;
};

#endif ///< ___FEC_ENCODER_SOFTRTP_H___
