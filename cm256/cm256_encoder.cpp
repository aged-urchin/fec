#include "cm256_encoder.h"
#include "cm256.h"
#include "cm256_header.h"
#include "../utils/utils.h"

#include <iostream>
#include <cassert>
#include <new>

CM256Encoder::~CM256Encoder() {
    destroy();
}

bool
CM256Encoder::create(FecConfig& config, uint16_t sequence, IFecEncoderAdapterObserver* observer) {
    assert(!m_params && !m_observer && observer);
    if (!init_cm256()) {
        return false;
    }

    m_params = new(std::nothrow) cm256_encoder_params_t;
    assert(m_params);

    m_params->BlockBytes    = config.block_size;
    m_params->OriginalCount = config.blocks;
    m_params->RecoveryCount = config.red_blocks;

    m_recovery_blocks.resize(config.block_size * config.red_blocks);

    m_sequence = sequence;
    m_observer = observer;

    return true;
}

void
CM256Encoder::destroy() {
    delete m_params;
    m_params = nullptr;
}

void
CM256Encoder::encode(const void* block, bool& done) {
    assert(m_params);
    if (m_num_blocks == m_params->OriginalCount) {
        std::cerr << "too many blocks, expected " << (int)m_params->OriginalCount << std::endl;
        done = true;
        return;
    }

    m_blocks.insert(m_blocks.end(), (uint8_t*)block, (uint8_t*)block + m_params->BlockBytes);
    ++m_num_blocks;

    on_new_block(m_num_blocks - 1, (uint8_t*)block);
    done = false;

    if (m_num_blocks == m_params->OriginalCount) {
        std::vector<cm256_block> blocks;
        for (int i = 0; i < m_num_blocks; ++i) {
            cm256_block cm_block;

            cm_block.Index = i;
            cm_block.Block = m_blocks.data() + i * m_params->BlockBytes;

            blocks.push_back(cm_block);
        }

        auto ret = cm256_encode(*m_params, blocks.data(), m_recovery_blocks.data());
        if (ret != 0) {
            std::cerr << "encoding error " << ret << std::endl;
            done = true;
            return;
        }

        for (int i = 0; i < m_params->RecoveryCount; ++i) {
            on_new_block(m_params->OriginalCount + i, m_recovery_blocks.data() + i * m_params->BlockBytes);
        }

        m_blocks.clear();
        done = true;
    }
}

void
CM256Encoder::on_new_block(uint8_t index, const uint8_t* data) {
    CM256Header header;

    header.s = UINT16_TO_BE((uint16_t)m_params->BlockBytes);
    header.n = (uint8_t)m_params->OriginalCount;
    header.k = (uint8_t)m_params->RecoveryCount;
    header.i = index;

    std::vector<uint8_t> block_data;
    block_data.insert(block_data.end(), (char*)&header, (char*)&header + sizeof(CM256Header));
    block_data.insert(block_data.end(), data, data + m_params->BlockBytes);

    m_observer->on_encoder_output(this, m_sequence, index, index >= m_params->OriginalCount, block_data.data(), (int32_t)block_data.size());
}
