#ifndef ___FEC_ENCODER_H___
#define ___FEC_ENCODER_H___

#include "fec_encoder_base.h"
#include <vector>

class FecEncoder : public FecEncoderBase {
public:
    FecEncoder(IFecEncoderObserver* observer);

    void encode(const uint8_t* data, int data_len) override;

private:
    void do_flush();

private:

    std::vector<uint8_t>    m_block;
};

#endif ///< ___BANDFEC_ENCODER_H___
