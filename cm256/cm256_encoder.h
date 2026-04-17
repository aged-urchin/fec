#ifndef ___CM256_ENCODER_H___
#define ___CM256_ENCODER_H___

#include "../fec_adapter.h"

#include <vector>

struct cm256_encoder_params_t;
class CM256Encoder : public IFecEncoderAdapter {
public:
    ~CM256Encoder() override;

    bool create(FecConfig& config, uint16_t sequence, IFecEncoderAdapterObserver* observer) override;

    void destroy() override;

    void encode(const void* block, bool& done) override;

private:
    void on_new_block(uint8_t index, const uint8_t* data);

private:

    IFecEncoderAdapterObserver*     m_observer{ nullptr };
    cm256_encoder_params_t*         m_params{ nullptr };
    uint16_t                        m_sequence{ 0 };
    uint8_t                         m_num_blocks{ 0 };
    std::vector<uint8_t>            m_blocks;
    std::vector<uint8_t>            m_recovery_blocks;
};

#endif ///< ___CM256_ENCODER_H___
