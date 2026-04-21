#ifndef ___LEOPARD_ENCODER_H___
#define ___LEOPARD_ENCODER_H___

#include "../fec_adapter.h"

class LeopardEncoder : public IFecEncoderAdapter {
public:
    ~LeopardEncoder() override;

    bool create(FecConfig& config, uint16_t sequence, IFecEncoderAdapterObserver* observer) override;

    void destroy() override;

    void encode(const void* block, bool& done) override;

private:
    bool store_new_block(const void* data);

    bool alloc_leo_blocks(std::vector<uint8_t*>& blocks, uint16_t num_blocks);

    void free_leo_blocks(std::vector<uint8_t*>& blocks);

    void on_new_block(uint16_t index, const uint8_t* data);

private:

    IFecEncoderAdapterObserver*     m_observer{ nullptr };
    uint16_t                        m_sequence{ 0 };
    uint16_t                        m_buffer_bytes{ 0 };
    uint16_t                        m_original_count{ 0 };
    uint16_t                        m_recovery_count{ 0 };
    uint16_t                        m_num_blocks{ 0 };
    std::vector<uint8_t*>           m_data_blocks;
};

#endif ///< ___LEOPARD_ENCODER_H___
