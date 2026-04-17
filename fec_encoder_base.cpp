#include "fec_encoder_base.h"
#include "fec_packet.h"
#include "bandfec/bandfec_encoder.h"
#include "cm256/cm256_encoder.h"
#include "./utils/utils.h"

#include <iostream>

FecEncoderBase::FecEncoderBase(FecType type, IFecEncoderObserver* observer) :
m_type(type),
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

    std::cerr << "flushing..." << std::endl;
    if (m_flushed) {
        std::cerr << "already flushed" << std::endl;
        return;
    }

    do_flush();

    destroy_encoder();
    m_flushed = true;
}

IFecEncoderAdapter*
FecEncoderBase::create_encoder() {
    IFecEncoderAdapter* encoder = nullptr;
    if (!m_observer ||
        0 == m_config.block_size || 0 == m_config.blocks || 0 == m_config.red_blocks) {
        std::cerr << "observer or param not set" << std::endl;
        return nullptr;
    }

    if (kFecTypeBand == m_type) {
        encoder = new(std::nothrow) BandFecEncoder();
    } else if (kFecTypeRS == m_type) {
        encoder = new(std::nothrow) CM256Encoder();
    }

    if (!encoder || !encoder->create(m_config, m_sequence_number++, this)) {
        std::cerr << "could not create encoder adapter" << std::endl;

        delete encoder;
        return nullptr;
    }

    if (!m_active_config.is_equal(m_config)) {
        std::cerr << "active config changed to " << m_config.to_string() << std::endl;
    }

    m_active_config = m_config;
    m_first_block   = true;

    return encoder;
}

void
FecEncoderBase::destroy_encoder() {
    if (m_encoder) {
        m_encoder->destroy();

        delete m_encoder;
        m_encoder = nullptr;
    }
}

bool
FecEncoderBase::do_set_block_size(int size_in_bytes) {
    auto alignment    = 16; ///< actually bandfec requires alignment on '4 * kFecParamG', while 'cm256' has no requirements
    auto aligned_size = (size_in_bytes + alignment - 1) & ~(alignment - 1);

    if (aligned_size <= (int)sizeof(FecFragmentHeader)) {
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

    m_config.blocks     = blocks_in_group;
    m_config.red_blocks = red_blocks_in_group;

    ///< std::cerr << "new red params set: " << m_config.to_string() << std::endl;
    return true;
}

void
FecEncoderBase::on_encoder_output(IFecEncoderAdapter* adapter, uint16_t sequence, int32_t index, bool red, const uint8_t* data, int32_t len) {
    if (m_first_block) {
        assert(0 == index);
        m_first_block = false;
    }

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
