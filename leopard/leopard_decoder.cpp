#include "leopard_decoder.h"
#include "leo_header.h"
#include "LeopardCommon.h"
#include "leopard.h"
#include "../utils/utils.h"

#include <algorithm>
#include <cassert>
#include <iostream>

LeopardDecoder::~LeopardDecoder() {
    destroy();
}

bool
LeopardDecoder::create(uint16_t sequence, IFecDecoderAdapterObserver* observer) {
    assert(!m_observer);
    assert(observer);

    if (!init_leopard()) {
        return false;
    }

    m_sequence = sequence;
    m_observer = observer;

    return true;
}

void
LeopardDecoder::destroy() {
    free_blocks(m_data_blocks);
    free_blocks(m_red_blocks);
}

void
LeopardDecoder::decode(const void* block) {
    if (m_finished) {
        return;
    }

    LeopardHeader header;
    leopard_parse_block(block, header);

    auto data = (uint8_t*)block + sizeof(LeopardHeader);
    auto len  = header.s;

    if (!m_last_header) {
        m_last_header = new(std::nothrow) LeopardHeader;
        assert(m_last_header);

        m_data_blocks.resize(header.n, nullptr);
        m_red_blocks.resize(header.k, nullptr);
    } else {
        assert(m_last_header->s == header.s && m_last_header->n == header.n && m_last_header->k == header.k);
        if (m_last_header->s != header.s || m_last_header->n != header.n || m_last_header->k != header.k) {
            return;
        }
    }

    *m_last_header = header;
    if (!store_new_block(&header, data, len)) {
        return;
    }

    if (header.i < header.n) {
        on_block_decoded(header.i, data, len);

        if (m_num_data_blocks == header.n) {
            m_finished = true;
            return;
        }
    }

    if (m_num_red_blocks != 0 &&
        m_num_data_blocks + m_num_red_blocks == header.n) {
        auto work_data = alloc_work_data_blocks(&header);
        if (work_data.empty()) {
            return;
        }

        auto ret = leo_decode(header.s,
                              header.n,
                              header.k,
                              (uint32_t)work_data.size(),
                              (void**)&m_data_blocks[0],
                              (void**)&m_red_blocks[0],
                              (void**)&work_data[0]);

        if (ret != Leopard_Success) {
            std::cerr << "decode failed(" << ret << "): " << leo_result_string(ret) << std::endl;
            return;
        }

        for (int i = 0; i < (int)header.n; ++i) {
            if (m_data_blocks[i]) {
                continue;
            }

            on_block_decoded(i, work_data[i], header.s);
        }

        free_blocks(work_data);
        m_finished = true;
    }
}

void
LeopardDecoder::flush() {
    /** no need to flush
     */
}

void
LeopardDecoder::parse(const void* buf, size_t size, FecHeaderInfo& info) {
    LeopardHeader header;
    leopard_parse_block(buf, header);

    info.s = header.s;
    info.n = header.n;
    info.k = header.k;
    info.i = header.i;

    info.pack = [](const FecHeaderInfo& header_info) -> std::vector<uint8_t> {
        LeopardHeader be_header;
        /** convert 'LeopardHeader' from host byte order to network byte order
         */
        be_header.s = UINT16_TO_BE(header_info.s);
        be_header.n = UINT16_TO_BE(header_info.n);
        be_header.k = UINT16_TO_BE(header_info.k);
        be_header.i = UINT16_TO_BE((uint16_t)header_info.i);

        return { (char*)&be_header, (char*)&be_header + sizeof(be_header) };
    };
}

bool
LeopardDecoder::store_new_block(LeopardHeader* header, const void* data, uint16_t block_size) {
    auto is_red = header->i >= header->n;
    auto index  = header->i - (is_red ? header->n : 0);

    assert(is_red || (index < m_data_blocks.size() && !m_data_blocks[index]));
    assert(!is_red || (index < m_red_blocks.size() && !m_red_blocks[index]));

    if (!is_red && (index >= m_data_blocks.size() || m_data_blocks[index])) {
        std::cerr << "invalid data index " << header->i << std::endl;
        return false;
    }

    if (is_red && (index >= m_red_blocks.size() || m_red_blocks[index])) {
        std::cerr << "invalid red index " << header->i << std::endl;
        return false;
    }

    auto leo_block = leopard::SIMDSafeAllocate(block_size);
    assert(leo_block);
    if (!leo_block) {
        return false;
    }

    memcpy(leo_block, data, block_size);
    if (is_red) {
        m_red_blocks[index] = leo_block;
        ++m_num_red_blocks;
    } else {
        m_data_blocks[index] = leo_block;
        ++m_num_data_blocks;
    }

    return true;
}

std::vector<uint8_t*>
LeopardDecoder::alloc_work_data_blocks(LeopardHeader* header) {
    auto work_count = leo_decode_work_count(header->n, header->k);

    std::vector<uint8_t*> work_data(work_count);
    for (unsigned i = 0; i < work_count; ++i) {
        work_data[i] = leopard::SIMDSafeAllocate(header->s);
        assert(work_data[i]);
        memset(work_data[i], 0, header->s);

        if (!work_data[i]) {
            std::cerr << "could not alloc work data, i: " << i << ", s: " << header->s << std::endl;
            free_blocks(work_data);
            return {};
        }
    }

    return work_data;
}

void
LeopardDecoder::free_blocks(std::vector<uint8_t*>& blocks) {
    std::for_each(blocks.begin(), blocks.end(), [](uint8_t* block) { if (block) { leopard::SIMDSafeFree(block); } });
    blocks.clear();
}

void
LeopardDecoder::on_block_decoded(int32_t index, const uint8_t* data, int len) {
    m_observer->on_decoder_output(this, m_sequence, index, data, len);
}
