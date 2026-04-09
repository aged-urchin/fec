#ifndef ___FEC_ENCODER_H___
#define ___FEC_ENCODER_H___

#include "fec_encoder_base.h"
#include <vector>

class FecEncoder : public FecEncoderBase {
public:
    FecEncoder(IFecEncoderObserver* observer);

private:
    void do_encode(const uint8_t* data, int data_len) override;

    void do_flush() override;

    FecHeader* make_fec_header(uint16_t sequence, bool red) override;

private:

    uint16_t                m_frame_num{ 0 };
    std::vector<uint8_t>    m_block;
};

#endif ///< ___BANDFEC_ENCODER_H___
