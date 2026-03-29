#include "fec_encoder.h"
#include "fec_packet.h"
#include "bandfec.h"

#include <iostream>
#include <cassert>

void
on_fec_send(BandFecEncoder* f, void* buf, size_t size, bool red, int64_t user_data1, int64_t user_data2) {
    auto encoder  = (FecEncoder*)user_data1;
    auto sequence = (uint16_t)user_data2;

    encoder->on_new_block(sequence, red, (uint8_t*)buf, size);
}

FecEncoder::FecEncoder(IFecEncoderObserver* observer) :
m_observer(observer) {

}

FecEncoder::~FecEncoder() {
    std::lock_guard<std::mutex> lock(m_mutex);

    destroy_encoder();
}

bool
FecEncoder::set_param(int block_size_in_bytes, int data_blocks_in_group, int redundant_blocks_in_group) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto aligned_size = (block_size_in_bytes / (4 * kFecParamG)) * (4 * kFecParamG);
    if (aligned_size <= sizeof(FecFragmentHeader) || data_blocks_in_group <= 0 || redundant_blocks_in_group <= 0) {
        std::cerr << "invalid arguments" << std::endl;
        return false;
    }

    m_config.block_size = aligned_size;
    m_config.blocks     = data_blocks_in_group;
    m_config.red_blocks = redundant_blocks_in_group;

    std::cerr << "new config set: " << m_config.to_string() << std::endl;
    return true;
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

    if (!m_observer || 0 == m_config.block_size) {
        std::cerr << "observer or param not set" << std::endl;
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
FecEncoder::flush() {
    if (!m_encoder) {
        return;
    }

    bool done = false;
    do {
        m_block.insert(m_block.end(), (char*)&kEndingFragHeader, (char*)&kEndingFragHeader + sizeof(FecFragmentHeader));

        bandfec_encode(m_encoder, (int32_t*)m_block.data(), done);
        m_block.clear();
    } while (!done);
    
    destroy_bandfec_encoder(m_encoder);
    m_encoder = nullptr;
}

BandFecEncoder*
FecEncoder::create_encoder() {
    auto encoder =
        create_bandfec_encoder(m_config.block_size,
                               m_config.blocks,
                               m_config.red_blocks,
                               kFecParamW,
                               kFecParamG,
                               &on_fec_send,
                               (int64_t)this,
                               m_group_num++); ///< 0, 1, 2, ...
    if (encoder) {
        if (!m_active_config.is_equal(m_config)) {
            std::cerr << "active config changed to " << m_config.to_string() << std::endl;
        }

        m_active_config = m_config;
    }
    return encoder;
}

void
FecEncoder::destroy_encoder() {
    if (m_encoder) {
        destroy_bandfec_encoder(m_encoder);
        m_encoder = nullptr;
    }
}

void
FecEncoder::on_new_block(uint16_t sequence, bool red, const uint8_t* data, int len) {
    auto packet = FecPacket::create_instance(data, len, sequence, red);
    if (!packet) {
        std::cerr << "could not create fec packet" << std::endl;
        return;
    }

    m_observer->on_encoder_output(this, packet);
    packet->release();
}
