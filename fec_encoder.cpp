#include "fec_encoder.h"
#include "bandfec.h"

#include <iostream>
#include <cassert>

FecEncoder::FecEncoder(IFecEncoderObserver* observer) :
FecEncoderBase(observer) {

}

void
FecEncoder::encode(const uint8_t* data, int data_len) {
    std::lock_guard<std::mutex> lock(m_mutex);

    FecFragmentHeader header;
    char* src      = (char*)data;
    int   src_size = data_len;

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

            auto be_header = header.to_network();
            /**  .__________________________________________.
             *   |                           |              |
             *   |     FecFragmentHeader     |     data     |
             *   |                           |              |
             *   `------------------------------------------`
             *   |<---------- Config.block_size ----------->|
             *
             */
            m_block.insert(m_block.end(), (char*)&be_header, (char*)&be_header + sizeof(FecFragmentHeader));
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
    bool done = false;
    do {
        m_block.insert(m_block.end(), (char*)&kEndingFragHeader, (char*)&kEndingFragHeader + sizeof(FecFragmentHeader));

        bandfec_encode(m_encoder, (int32_t*)m_block.data(), done);
        m_block.clear();
    } while (!done);
}
