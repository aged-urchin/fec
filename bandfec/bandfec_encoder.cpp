#include "bandfec_encoder.h"
#include "bandfec.h"

#include <cassert>

void
on_fec_send(BandFecEnc* f, void* buf, size_t size, bool red, int64_t user_data1, int64_t user_data2) {
    auto encoder  = (BandFecEncoder*)user_data1;
    auto sequence = (uint16_t)user_data2;

    encoder->on_new_block(sequence, red, (uint8_t*)buf, (int)size);
}

BandFecEncoder::~BandFecEncoder() {
    destroy();
}

bool
BandFecEncoder::create(FecConfig& config, uint16_t sequence, IFecEncoderAdapterObserver* observer) {
    assert(!m_encoder && !m_observer);
    assert(observer);

    m_encoder =
        create_bandfec_encoder(config.block_size,
                               config.blocks,
                               config.red_blocks,
                               kFecParamW,
                               kFecParamG,
                               &on_fec_send,
                               (int64_t)this,
                               sequence);
    if (!m_encoder) {
        return false;
    }

    m_observer = observer;
    return true;
}

void
BandFecEncoder::destroy() {
    if (!m_encoder) {
        return;
    }

    destroy_bandfec_encoder(m_encoder);
    m_encoder = nullptr;
}

void
BandFecEncoder::encode(const void* block, bool& done) {
    if (!m_encoder) {
        return;
    }

    bandfec_encode(m_encoder, (int32_t*)block, done);
}

void
BandFecEncoder::on_new_block(uint16_t sequence, bool red, const uint8_t* data, int len) {
    BandFecHeaderType bandfec_header;
    bandfec_parse_block((void*)data, bandfec_header);

    m_observer->on_encoder_output(this, sequence, bandfec_header.i, red, data, len);
}
