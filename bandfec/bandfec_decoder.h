#ifndef ___BANDFEC_DECODER_H___
#define ___BANDFEC_DECODER_H___

#include "../fec_adapter.h"

struct BandFecDec;
class BandFecDecoder : public IFecDecoderAdapter {
public:
    ~BandFecDecoder() override;

    bool create(uint16_t sequence, IFecDecoderAdapterObserver* observer) override;

    void destroy() override;

    void decode(const void* block) override;

    void flush() override;

    static void parse(const void* buf, size_t size, FecHeaderInfo& info);

private:
    void on_block_decoded(uint16_t sequence_number, int32_t index, const uint8_t* data, int len);

private:
    friend void on_fec_receive(BandFecDec* f, int64_t position, void* buf, int len, int64_t user_data1, int64_t user_data2);

    IFecDecoderAdapterObserver*     m_observer{ nullptr };
    BandFecDec*                     m_decoder{ nullptr };
};

#endif ///< ___BANDFEC_DECODER_H___
