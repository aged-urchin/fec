#include "bandfec_encoder.h"
#include "bandfec.h"

#include <iostream>

uint32_t
get_block_index(const uint8_t* buf, size_t size) {
    HeaderType header;
    fec_parse_block((void*)buf, size, header);

    return header.i;
}

void
on_fec_send(FecEncoder* f, void* buf, size_t size, int64_t user_data1, int64_t user_data2) {
    auto encoder  = (BandFecEncoder*)user_data1;
    auto sequence = (uint32_t)user_data2;

    if (user_data2 > UINT32_MAX) {
        std::cerr << "user_data2 is too large as an uint32" << std::endl;
    }
    encoder->on_new_block(sequence, (uint8_t*)buf, size);
}

BandFecEncoder::GroupEncoder::GroupEncoder(const Config& _config) :
config(_config) {

}

BandFecEncoder::GroupEncoder::~GroupEncoder() {
    destroy_fec_encoder(encoder);
}

bool
BandFecEncoder::GroupEncoder::is_same(const Config& _config) const {
    return _config.block_size == config.block_size && _config.blocks == config.blocks && _config.red_blocks == config.red_blocks;
}

BandFecEncoder::BandFecEncoder(IBandFecEncoderObserver* observer) :
m_observer(observer) {

}

BandFecEncoder::~BandFecEncoder() {
    std::lock_guard<std::mutex> lock(m_mutex);

    delete m_main_encoder;
    m_main_encoder = nullptr;

    delete m_secondary_encoder;
    m_secondary_encoder = nullptr;
}

void
BandFecEncoder::set_param(int block_size_in_bytes, int data_blocks_in_group, int redundant_blocks_in_group) {
    std::lock_guard<std::mutex> lock(m_mutex);

    GroupEncoder::Config config;

    config.block_size = block_size_in_bytes;
    config.blocks     = data_blocks_in_group;
    config.red_blocks = redundant_blocks_in_group;

    auto create_encoder = [config, this]() {
        static const int kFecParamW = 40;
        static const int kFecParamG = 4;

        auto encoder = new GroupEncoder(config);
        encoder->encoder = create_fec_encoder(config.block_size,
                                              config.blocks,
                                              config.red_blocks,
                                              kFecParamW,
                                              kFecParamG,
                                              &on_fec_send,
                                              (int64_t)this,
                                              m_sequence++);

        return encoder;
    };

    if ((m_secondary_encoder && m_secondary_encoder->is_same(config)) ||
        (m_main_encoder && m_main_encoder->is_same(config))) {
        return;
    }

    auto encoder = create_encoder();
    if (!m_main_encoder) {
        m_main_encoder = encoder;
    } else {
        delete m_secondary_encoder;
        m_secondary_encoder = encoder;
    }
}

void
BandFecEncoder::encode(const uint8_t* data, int data_len) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_main_encoder) {
        return;
    }

    m_main_encoder->block.insert(m_main_encoder->block.end(), data, data + data_len);
    
    while (m_main_encoder->block.size() >= m_main_encoder->config.block_size) {
        fec_encode(m_main_encoder->encoder, (int32_t*)&m_main_encoder->block);

        m_main_encoder->block.erase(m_main_encoder->block.begin(), m_main_encoder->block.begin() + m_main_encoder->config.block_size);

        if (++m_main_encoder->block_idx == m_main_encoder->config.blocks) {
            if (m_secondary_encoder) {
                m_secondary_encoder->block = std::move(m_main_encoder->block);
                delete m_main_encoder;

                m_main_encoder = m_secondary_encoder;
                m_secondary_encoder = nullptr;
            } else {
                m_main_encoder->block_idx = 0;
            }
        }
    }
}

void
BandFecEncoder::on_new_block(uint32_t sequence, const uint8_t* data, int len) {
    m_observer->on_encoder_output(this, sequence, data, len);
}
