#ifndef ___LEOPARD_DECODER_H___
#define ___LEOPARD_DECODER_H___

#include "../fec_adapter.h"
#include "leo_header.h"

class LeopardDecoder : public IFecDecoderAdapter {
public:
    ~LeopardDecoder() override;

    bool create(uint16_t sequence, IFecDecoderAdapterObserver* observer) override;

    void destroy() override;

    void decode(const void* block) override;

    void flush() override;

    static void parse(const void* buf, size_t size, FecHeaderInfo& info);

private:
    bool store_new_block(LeopardHeader* header, const void* data, uint16_t block_size);

    std::vector<uint8_t*> alloc_work_data_blocks(LeopardHeader* header);

    void free_blocks(std::vector<uint8_t*>& blocks);

    void on_block_decoded(int32_t index, const uint8_t* data, int len);

private:

    IFecDecoderAdapterObserver*     m_observer{ nullptr };
    LeopardHeader*                  m_last_header{ nullptr };
    uint16_t                        m_sequence{ 0 };
    bool                            m_finished{ false };
    std::vector<uint8_t*>           m_data_blocks;
    std::vector<uint8_t*>           m_red_blocks;
    uint16_t                        m_num_data_blocks{ 0 };
    uint16_t                        m_num_red_blocks{ 0 };
};

#endif ///< ___LEOPARD_DECODER_H___
