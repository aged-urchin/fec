#include "leopard_encoder.h"
#include "leo_header.h"
#include "LeopardCommon.h"
#include "leopard.h"
#include "../utils/utils.h"

#include <cassert>
#include <iostream>
#include <algorithm>

LeopardEncoder::~LeopardEncoder() {
    destroy();
}

bool
LeopardEncoder::create(FecConfig& config, uint16_t sequence, IFecEncoderAdapterObserver* observer) {
    assert(!m_observer && observer);
    assert(0 == config.block_size % 64);
    assert(config.red_blocks <= config.blocks);
    assert(config.blocks + config.red_blocks <= 65536);

    if (!init_leopard()) {
        std::cerr << "could not init leopard" << std::endl;
        return false;
    }

    m_buffer_bytes   = config.block_size;
    m_original_count = config.blocks;
    m_recovery_count = config.red_blocks;

    m_sequence = sequence;
    m_observer = observer;

    return true;
}

void
LeopardEncoder::destroy() {
    std::for_each(m_data_blocks.begin(), m_data_blocks.end(), [](uint8_t* block) { leopard::SIMDSafeFree(block); });
    m_data_blocks.clear();
}

void
LeopardEncoder::encode(const void* block, bool& done) {
    if (m_num_blocks == m_original_count) {
        std::cerr << "too many blocks, expected " << m_original_count << std::endl;
        done = true;
        return;
    }

    if (!store_new_block(block)) {
        std::cerr << "could not alloc for new block" << std::endl;
        done = false;
        return;
    }

    on_new_block(m_num_blocks - 1, (uint8_t*)block);
    done = false;

    if (m_data_blocks.size() == m_original_count) {
        std::vector<uint8_t*> work_data_blocks;

        auto encode_work_count = leo_encode_work_count(m_original_count, m_recovery_count);
        if (!alloc_leo_blocks(work_data_blocks, encode_work_count)) {
            std::cerr << "could not allocate recovery blocks" << std::endl;
            done = true;
            return;
        }

        auto ret = leo_encode(m_buffer_bytes,
                              m_original_count,
                              m_recovery_count,
                              (uint32_t)work_data_blocks.size(),
                              (void**)&m_data_blocks[0],
                              (void**)&work_data_blocks[0]);
        if (ret != Leopard_Success) {
            std::cerr << "encode failed with(" << ret << "): " << leo_result_string(ret) << std::endl;

            free_leo_blocks(work_data_blocks);
            done = true;
            return;
        }

        /** the first set of recovery_count buffers in work_data will be the result
         */
        for (int i = 0; i < m_recovery_count; ++i) {
            on_new_block(m_original_count + i, work_data_blocks[i]);
        }

        free_leo_blocks(m_data_blocks);
        free_leo_blocks(work_data_blocks);
        done = true;
    }
}

bool
LeopardEncoder::store_new_block(const void* block) {
    auto leo_block = leopard::SIMDSafeAllocate(m_buffer_bytes);
    assert(leo_block);
    if (!leo_block) {
        return false;
    }

    memcpy(leo_block, block, m_buffer_bytes);

    m_data_blocks.push_back(leo_block);
    ++m_num_blocks;

    return true;
}

bool
LeopardEncoder::alloc_leo_blocks(std::vector<uint8_t*>& blocks, uint16_t num_blocks) {
    for (int i = 0; i < num_blocks; ++i) {
        auto block = leopard::SIMDSafeAllocate(m_buffer_bytes);
        assert(block);
        if (!block) {
            return false;
        }

        blocks.push_back(block);
    }

    return true;
}

void
LeopardEncoder::free_leo_blocks(std::vector<uint8_t*>& blocks) {
    std::for_each(blocks.begin(), blocks.end(), [](uint8_t* block) { leopard::SIMDSafeFree(block); });
    blocks.clear();
}

void
LeopardEncoder::on_new_block(uint16_t index, const uint8_t* data) {
    LeopardHeader header;

    header.s = UINT16_TO_BE(m_buffer_bytes);
    header.n = UINT16_TO_BE(m_original_count);
    header.k = UINT16_TO_BE(m_recovery_count);
    header.i = UINT16_TO_BE(index);

    std::vector<uint8_t> block_data;
    block_data.insert(block_data.end(), (char*)&header, (char*)&header + sizeof(LeopardHeader));
    block_data.insert(block_data.end(), data, data + m_buffer_bytes);

    m_observer->on_encoder_output(this, m_sequence, index, index >= m_original_count, block_data.data(), (int32_t)block_data.size());
}
