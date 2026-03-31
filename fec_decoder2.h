#ifndef ___FEC_DECODER2_H___
#define ___FEC_DECODER2_H___

#include "fec_decoder_base.h"

class FecDecoder2 : public FecDecoderBase {
public:
    FecDecoder2(IFecDecoderObserver* observer);

    ~FecDecoder2() override;

private:
    void on_new_block(uint16_t sequence_number, int32_t pos, const uint8_t* data, int len);

private:
    
};

#endif ///< ___FEC_DECODER2_H___
