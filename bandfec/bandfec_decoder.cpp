#include "bandfec_decoder.h"
#include "bandfec.h"
#include "band_header.h"
#include "../utils/utils.h"

#include <cassert>
#include <iostream>

void
on_fec_receive(BandFecDec* f, int64_t position, void* buf, int len, int64_t user_data1, int64_t user_data2) {
    auto decoder  = (BandFecDecoder*)user_data1;
    auto sequence = (uint32_t)user_data2;

    if (position % len) {
        std::cerr << "invalid position " << position << ", block size is " << len << std::endl;
    }

    decoder->on_block_decoded(sequence, (int32_t)position / len, (const uint8_t*)buf, len);
}

BandFecDecoder::~BandFecDecoder() {
    destroy();
}

bool
BandFecDecoder::create(uint16_t sequence, IFecDecoderAdapterObserver* observer) {
    assert(!m_observer);
    assert(observer);

    m_decoder = create_bandfec_decoder(&on_fec_receive, (int64_t)this, sequence);
    if (!m_decoder) {
        return false;
    }

    m_observer = observer;
    return true;
}

void
BandFecDecoder::destroy() {
    destroy_bandfec_decoder(m_decoder);
    m_decoder = nullptr;
}

void
BandFecDecoder::decode(const void* block) {
    if (!m_decoder) {
        return;
    }

    bandfec_decode(m_decoder, (void*)block);
}

void
BandFecDecoder::flush() {
    flush_bandfec_decoder(m_decoder);
}

void
BandFecDecoder::on_block_decoded(uint16_t sequence_number, int32_t index, const uint8_t* data, int len) {
    m_observer->on_decoder_output(this, sequence_number, index, data, len);
}

void
BandFecDecoder::parse(const void* buf, size_t, FecHeaderInfo& info) {
    BandFecHeader header;
    bandfec_parse_block((void*)buf, header);

    info.s = header.s;
    info.n = header.n;
    info.k = header.k;
    info.i = header.i;

    info.header_size = sizeof(BandFecHeader);

    info.pack = [w=header.w, g=header.g](const FecHeaderInfo& header_info) {
        BandFecHeader be_header;
        /** convert 'BandFecHeaderType' from host byte order to network byte order
         */
        be_header.s = UINT16_TO_BE((header_info.s));
        be_header.n = UINT16_TO_BE((header_info.n));
        be_header.k = UINT16_TO_BE((header_info.k));
        be_header.i = UINT32_TO_BE((header_info.i));
        be_header.w = w;
        be_header.g = g;

        std::vector<uint8_t> data;
        data.insert(data.end(), (char*)&be_header, (char*)&be_header + sizeof(be_header));

        return data;
    };
}
