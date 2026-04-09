#include "fec_encoder.h"
#include "bandfec.h"
#include "utils.h"

#include <iostream>
#include <cassert>

FecEncoder::FecEncoder(IFecEncoderObserver* observer) :
FecEncoderBase(observer) {

}

void
FecEncoder::do_encode(const uint8_t* data, int data_len) {
    FecFragmentHeader header{ 0 };
    auto src      = (uint8_t*)data;
    auto src_size = data_len;

    if (!data || data_len <= 0) {
        std::cerr << "invalid arguments" << std::endl;
        return;
    }

    header.frame_number = m_frame_num++; ///< 0, 1, 2, ...
    header.frame_size   = data_len;
    header.frag_offset  = 0;
    header.frag_size    = 0;

    while (src_size > 0) {
        if (!m_encoder && !(m_encoder = create_encoder())) {
            std::cerr << "could not create encoder" << std::endl;
            return;
        }

        auto available_size = m_active_config.block_size - (int)m_block.size();
        assert(available_size > 0);

        auto data_size = available_size - (int)sizeof(FecFragmentHeader);
        if (data_size > 0) {
            auto copy_size = (std::min)(src_size, data_size);

            header.frag_offset += header.frag_size;
            header.frag_size    = copy_size;

            auto be_header = convert_fragment_to_network(&header);
            /**  .__________________________________________.
             *   |                           |              |
             *   |     FecFragmentHeader     |     data     |
             *   |                           |              |
             *   `------------------------------------------`
             *   |<---------- Config.block_size ----------->|
             *
             */
            m_block.insert(m_block.end(), (uint8_t*)&be_header, (uint8_t*)&be_header + sizeof(FecFragmentHeader));
            m_block.insert(m_block.end(), src, src + copy_size);

            src      += copy_size;
            src_size -= copy_size;
        }

        if (m_active_config.block_size - m_block.size() > sizeof(FecFragmentHeader)) {
            /** there is still enough space for another writing(at least one extra data byte(excluding the header))
             */
            assert(0 == src_size);
            break;
        }
        /** the trailing trivial bytes(the unused ending bytes that equal to or less than sizeof(FecFragmentHeader)) if present, will be ignored by the decoder
         */

        /** block is full, or remaining space is not enough for another writing: encode this block
         */
        bool done = false;
        bandfec_encode(m_encoder, (int32_t*)m_block.data(), done);
        m_block.clear();

        if (done) {
            destroy_bandfec_encoder(m_encoder);
            m_encoder = nullptr;
        }
    }
}

void
FecEncoder::do_flush() {
    if (!m_encoder) {
        return;
    }

    bool done = false;
    do {
        m_block.insert(m_block.end(), (uint8_t*)&kEndingFragHeader, (uint8_t*)&kEndingFragHeader + sizeof(FecFragmentHeader));

        bandfec_encode(m_encoder, (int32_t*)m_block.data(), done);
        m_block.clear();
    } while (!done);
}

FecHeader*
FecEncoder::make_fec_header(uint16_t sequence, bool red) {
    auto header = create_fec_header();

    header->sig = 0b11; ///< 3
    header->typ = kFecExtNull;
    header->sid = 0;
    header->red = red ? 1 : 0;

    header->reserved        = 0;
    header->sequence_number = sequence;

    return header;
}
