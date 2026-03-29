#include "fec_encoder_base.h"
#include "fec_packet.h"
#include "bandfec.h"

#include <iostream>
#include <cassert>

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
FecEncoderBase::set_param(int block_size_in_bytes, int data_blocks_in_group, int redundant_blocks_in_group) {
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

BandFecEncoder*
FecEncoderBase::create_encoder() {
    if (!m_observer || 0 == m_config.block_size) {
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
FecEncoderBase::flush() {
    if (!m_encoder) {
        return;
    }

    do_flush();

    destroy_bandfec_encoder(m_encoder);
    m_encoder = nullptr;
}

void
FecEncoderBase::destroy_encoder() {
    if (m_encoder) {
        destroy_bandfec_encoder(m_encoder);
        m_encoder = nullptr;
    }
}

void
FecEncoderBase::on_new_block(uint16_t sequence, bool red, const uint8_t* data, int len) {
    auto packet = FecPacket::create_instance(data, len, sequence, red);
    if (!packet) {
        std::cerr << "could not create fec packet" << std::endl;
        return;
    }

    m_observer->on_encoder_output(this, packet);
    packet->release();
}
