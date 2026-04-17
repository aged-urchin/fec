#ifndef ___CM256_DECODER_H___
#define ___CM256_DECODER_H___

#include "../fec_adapter.h"
#include "header.h"
#include <set>

struct cm256_block_t;
class CM256Decoder : public IFecDecoderAdapter {
public:
    ~CM256Decoder() override;

    bool create(uint16_t sequence, IFecDecoderAdapterObserver* observer) override;

    void destroy() override;

    void decode(const void* block) override;

    void flush() override;

    static void parse(const void* buf, size_t size, FecHeaderInfo& info);

private:
    void on_block_decoded(int32_t index, const uint8_t* data, int len);

    cm256_block_t* create_cm256_block(const void* data, int len, int index);

    void destroy_cm256_block(cm256_block_t* block);

private:

    IFecDecoderAdapterObserver*     m_observer{ nullptr };
    CM256Header*                    m_last_header{ nullptr };
    uint16_t                        m_sequence{ 0 };
    bool                            m_finished{ false };
    std::vector<cm256_block_t*>     m_data_blocks;
    std::vector<cm256_block_t*>     m_red_blocks;
};

#endif ///< ___CM256_DECODER_H___
