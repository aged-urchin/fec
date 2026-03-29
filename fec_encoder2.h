#ifndef ___FEC_ENCODER2_H___
#define ___FEC_ENCODER2_H___

#include "fec_encoder_base.h"

class FecEncoder2 : public FecEncoderBase {
public:
    FecEncoder2(IFecEncoderObserver* observer);

    void encode(const uint8_t* data, int data_len) override;

private:

    void do_flush() override;
};

#endif ///< ___FEC_ENCODER2_H___
