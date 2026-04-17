#include "cm256_decoder.h"
#include "cm256.h"
#include "../utils/utils.h"

#include <cassert>
#include <iostream>
#include <algorithm>
#include <string.h>
#include <new>

CM256Decoder::~CM256Decoder() {
    destroy();
}

bool
CM256Decoder::create(uint16_t sequence, IFecDecoderAdapterObserver* observer) {
    assert(!m_observer);
    assert(observer);

    if (!init_cm256()) {
        return false;
    }

    m_sequence = sequence;
    m_observer = observer;

    return true;
}

void
CM256Decoder::destroy() {
    delete m_last_header;
    m_last_header = nullptr;

    std::for_each(m_data_blocks.begin(), m_data_blocks.end(), [this](cm256_block_t* block) { destroy_cm256_block(block); });
    m_data_blocks.clear();

    std::for_each(m_red_blocks.begin(), m_red_blocks.end(), [this](cm256_block_t* block) { destroy_cm256_block(block); });
    m_red_blocks.clear();
}

void
CM256Decoder::decode(const void* block) {
    if (m_finished) {
        return;
    }

    CM256Header header;
    cm256_parse_block(block, header);

    auto data      = (uint8_t*)block + sizeof(CM256Header);
    auto len       = header.s;
    auto new_block = create_cm256_block(data, len, header.i);

    if (!m_last_header) {
        m_last_header = new(std::nothrow) CM256Header;
        assert(m_last_header);
    } else {
        assert(m_last_header->s == header.s && m_last_header->n == header.n && m_last_header->k == header.k);
        if (m_last_header->s != header.s || m_last_header->n != header.n || m_last_header->k != header.k) {
            return;
        }
    }

    *m_last_header = header;

    if (header.i < header.n) {
        on_block_decoded(header.i, data, len);

        m_data_blocks.push_back(new_block);
        if (m_data_blocks.size() == header.n) {
            m_finished = true;
            return;
        }
    } else {
        m_red_blocks.push_back(new_block);
    }

    if (!m_red_blocks.empty() &&
        m_data_blocks.size() + m_red_blocks.size() == header.n) {
        cm256_encoder_params params;

        params.BlockBytes    = header.s;
        params.OriginalCount = header.n;
        params.RecoveryCount = header.k;

        std::vector<cm256_block_t> blocks;
        for (auto& block : m_data_blocks) {
            blocks.push_back(*block);
        }

        for (auto& block : m_red_blocks) {
            blocks.push_back(*block);
        }

        auto ret = cm256_decode(params, blocks.data());
        if (ret != 0) {
            std::cerr << "decoding error " << ret << std::endl;
            /** remove a red block and try next time
             */
            auto red_block = m_red_blocks.back();
            m_red_blocks.pop_back();

            destroy_cm256_block(red_block);
            return;
        }

        for (int i = 0; i < (int)m_red_blocks.size(); ++i) {
            on_block_decoded(blocks[m_data_blocks.size() + i].Index, (uint8_t*)blocks[m_data_blocks.size() + i].Block, header.s);
        }

        m_finished = true;
    }
}

void
CM256Decoder::flush() {
    /** nothing to flush
     */
}

void
CM256Decoder::on_block_decoded(int32_t index, const uint8_t* data, int len) {
    m_observer->on_decoder_output(this, m_sequence, index, data, len);
}

cm256_block_t*
CM256Decoder::create_cm256_block(const void* data, int len, int index) {
    auto block = (cm256_block_t*) new(std::nothrow) uint8_t[sizeof(cm256_block_t) + len];
    assert(block);

    block->Index = index;
    block->Block = (void*)((uint8_t*)block + sizeof(cm256_block_t));

    memcpy(block->Block, data, len);
    return block;
}

void
CM256Decoder::destroy_cm256_block(cm256_block_t* block) {
    delete[] (uint8_t*)block;
}

void
CM256Decoder::parse(const void* buf, size_t, FecHeaderInfo& info) {
    CM256Header header;
    cm256_parse_block(buf, header);

    info.s = header.s;
    info.n = header.n;
    info.k = header.k;
    info.i = header.i;

    info.pack = [](const FecHeaderInfo& header_info) -> std::vector<uint8_t> {
        CM256Header be_header;
        /** convert 'CM256Header' from host byte order to network byte order
         */
        be_header.s = UINT16_TO_BE(header_info.s);
        be_header.n = (uint8_t)header_info.n;
        be_header.k = (uint8_t)header_info.k;
        be_header.i = (uint8_t)header_info.i;

        return { (char*)&be_header, (char*)&be_header + sizeof(be_header) };
    };
}
