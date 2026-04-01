#include "fec_decoder2.h"
#include <iostream>

FecDecoder2::FecDecoder2(IFecDecoderObserver* observer) :
FecDecoderBase(observer) {

}

FecDecoder2::~FecDecoder2() {
    destroy_decoders();
}

void
FecDecoder2::decode(const uint8_t* data, int len) {
    if (is_rtp(data, len)) {

    }

    FecDecoderBase::decode(data, len);
}

bool
FecDecoder2::is_rtp(const uint8_t* data, size_t len) {
    return len >= 12 && 0x80 == (data[0] & 0xC0);
}

void
FecDecoder2::on_sequence_start(uint16_t sequence, const FecHeader* header, int block_size) {
    assert(0 == m_rtp_packet_size.count(sequence));
    assert(1 == header->typ);
    
    auto rtp_ext = (RtpFecExt*) header->ext;
    assert(rtp_ext);
    assert(rtp_ext->delta_size_bytes > 0);

    auto ptr = (uint8_t*)rtp_ext->delta_size;

    int remaining_bytes = rtp_ext->delta_size_bytes;
    while (remaining_bytes > 0) {
        auto is_one_byte_len = 0 == (*ptr & 0x80);
        uint16_t delta_size = 0;

        if (is_one_byte_len) {
            delta_size = *ptr++;
        } else {
            delta_size |= ((uint8_t)(*ptr++ & 0x7f) << 8);
            delta_size |= *ptr++;
        }

        m_rtp_packet_size[sequence][rtp_ext->base_sequence_num++] = block_size - delta_size;

        remaining_bytes -= (is_one_byte_len ? 1 : 2);
    }
}

void
FecDecoder2::on_sequence_end(uint16_t sequence) {
    m_rtp_packet_size.erase(sequence);
}

void
FecDecoder2::on_new_block(uint16_t sequence_number, int32_t pos, const uint8_t* data, int len) {
    RtpHeader rtp_header;
    if (!parse_rtp_buffer(data, len, rtp_header)) {
        std::cerr << "decoder outputs an invalid rtp packet" << std::endl;
        return;
    }

    assert(m_rtp_packet_size.count(sequence_number) != 0);
    assert(m_rtp_packet_size[sequence_number].count(rtp_header.seq) != 0);
    assert(m_rtp_packet_size[sequence_number][rtp_header.seq] <= len);

    /** use 'rtp sequence number' as the 2nd param?
     */
    auto rtp_packet_size = m_rtp_packet_size[sequence_number][rtp_header.seq];
    send_frame(sequence_number, rtp_header.seq, data, rtp_packet_size);

    ///< std::cerr << "decode seq: " << rtp_header.seq << ", len: " << rtp_packet_size << std::endl;
}
