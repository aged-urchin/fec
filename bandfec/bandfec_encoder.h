#ifndef ___BANDFEC_ENCODER_H___
#define ___BANDFEC_ENCODER_H___

#include "../fec_adapter.h"

struct BandFecEnc;
class BandFecEncoder : public IFecEncoderAdapter {
public:
    ~BandFecEncoder() override;

    bool create(FecConfig& config, uint16_t sequence, IFecEncoderAdapterObserver* observer) override;

    void destroy() override;

    void encode(const void* block, bool& done) override;

private:
    void on_new_block(uint16_t sequence, bool red, const uint8_t* data, int len);

private:
    friend void on_fec_send(BandFecEnc* f, void* buf, size_t size, bool red, int64_t user_data1, int64_t user_data2);

    enum {
        kFecParamW = 40, kFecParamG = 4,
    };

    IFecEncoderAdapterObserver*     m_observer{ nullptr };
    BandFecEnc*                     m_encoder{ nullptr };
};

#endif ///< ___BANDFEC_ENCODER_ADAPTER_H___
