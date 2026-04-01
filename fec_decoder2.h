#ifndef ___FEC_DECODER2_H___
#define ___FEC_DECODER2_H___

#include "fec_decoder_base.h"

class FecDecoder2 : public FecDecoderBase {
public:
    FecDecoder2(IFecDecoderObserver* observer);

    ~FecDecoder2() override;

    void decode(const uint8_t* data, int len) override;

private:
    bool is_rtp(const uint8_t* data, size_t len);

    void on_sequence_start(uint16_t sequence, const FecHeader* header, int block_size) override;

    void on_sequence_end(uint16_t sequence) override;

    void on_new_block(uint16_t sequence_number, int32_t pos, const uint8_t* data, int len) override;

private:
    
    std::map<uint16_t, std::map<uint16_t, uint16_t>> m_rtp_packet_size;
};

#endif ///< ___FEC_DECODER2_H___
