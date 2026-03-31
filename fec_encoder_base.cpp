#include "fec_encoder_base.h"
#include "fec_packet.h"
#include "bandfec.h"

#include <iostream>

void
on_fec_send(BandFecEncoder* f, void* buf, size_t size, bool red, int64_t user_data1, int64_t user_data2) {
    auto encoder  = (FecEncoderBase*)user_data1;
    auto sequence = (uint16_t)user_data2;

    encoder->on_new_block(sequence, red, (uint8_t*)buf, size);
}

FecEncoderBase::FecEncoderBase(IFecEncoderObserver* observer) :
m_observer(observer) {

}

FecEncoderBase::~FecEncoderBase() {
    std::lock_guard<std::mutex> lock(m_mutex);

    destroy_encoder();
}

bool
FecEncoderBase::set_block_size(int size_in_bytes) {
    std::lock_guard<std::mutex> lock(m_mutex);

    return do_set_block_size(size_in_bytes);
}

bool
FecEncoderBase::set_red_params(int blocks_in_group, int red_blocks_in_group) {
    std::lock_guard<std::mutex> lock(m_mutex);

    return do_set_red_params(blocks_in_group, red_blocks_in_group);
}

void
FecEncoderBase::encode(const uint8_t* data, int data_len) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_flushed) {
        std::cerr << "could not encode after flush" << std::endl;
        return;
    }

    do_encode(data, data_len);
}

void
FecEncoderBase::flush() {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::cerr << "flushing ..." << std::endl;
    if (m_flushed) {
        std::cerr << "already flushed" << std::endl;
        return;
    }

    do_flush();

    if (m_encoder) {
        destroy_bandfec_encoder(m_encoder);
        m_encoder = nullptr;
    }

    m_flushed = true;
}

BandFecEncoder*
FecEncoderBase::create_encoder() {
    if (!m_observer ||
        0 == m_config.block_size || 0 == m_config.blocks || 0 == m_config.red_blocks) {
        std::cerr << "observer or param not set" << std::endl;
        return nullptr;
    }

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
FecEncoderBase::destroy_encoder() {
    if (m_encoder) {
        destroy_bandfec_encoder(m_encoder);
        m_encoder = nullptr;
    }
}

bool
FecEncoderBase::do_set_block_size(int size_in_bytes) {
    auto alignment = 4 * kFecParamG;
    auto aligned_size = (size_in_bytes + alignment - 1) & ~(alignment - 1);

    if (aligned_size <= sizeof(FecFragmentHeader)) {
        std::cerr << "invalid arguments" << std::endl;
        return false;
    }

    m_config.block_size = aligned_size;

    ///< std::cerr << "new block size set: " << m_config.to_string() << std::endl;
    return true;
}

bool
FecEncoderBase::do_set_red_params(int blocks_in_group, int red_blocks_in_group) {
    if (blocks_in_group <= 0 || red_blocks_in_group <= 0) {
        std::cerr << "invalid arguments" << std::endl;
        return false;
    }

    m_config.blocks = blocks_in_group;
    m_config.red_blocks = red_blocks_in_group;

    ///< std::cerr << "new red params set: " << m_config.to_string() << std::endl;
    return true;
}

void
FecEncoderBase::on_new_block(uint16_t sequence, bool red, const uint8_t* data, int len) {
    auto header = make_fec_header(sequence, red);
    if (!header) {
        return;
    }

    auto packet = FecPacket::create_instance(header, data, len);
    if (!packet) {
        std::cerr << "could not create fec packet" << std::endl;
        return;
    }

    destroy_fec_header(header);

    m_observer->on_encoder_output(this, packet);
    packet->release();
}
