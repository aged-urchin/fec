#include "fec_encoder_softrtp.h"
#include "./utils/utils.h"

#include <iostream>
#include <cassert>

FecEncoderSoftRtp::FecEncoderSoftRtp(FecType type, IFecEncoderObserver* observer) :
FecEncoderBase(type, observer) {

}

bool
FecEncoderSoftRtp::set_red_params(int blocks_in_group, int red_blocks_in_group) {
    /** 2: kRtpFecExtTwoByteSizeMax
     */
    if (blocks_in_group > UINT8_MAX / 2) {
        std::cerr << "invalid arguments, blocks(" << blocks_in_group << ") too large for 'kFecExtRtp'" << std::endl;
        return false;
    }

    return FecEncoderBase::set_red_params(blocks_in_group, red_blocks_in_group);
}

void
FecEncoderSoftRtp::do_encode(const uint8_t* data, int data_len) {
    RtpHeader rtp_header;
    if (!parse_rtp_buffer(data, data_len, rtp_header)) {
        std::cerr << "invalid rtp packet" << std::endl;
        return;
    }

    ///< std::cerr << "encode seq: " << rtp_header.seq << ", len: " << data_len << std::endl;
    if (!m_packets.empty()) {
        if (m_ssrc != rtp_header.ssrc) {
            std::cerr << "ssrc differs, was: " << m_ssrc << ", new: " << rtp_header.ssrc << std::endl;
            return;
        }

        if (data_len > kSoftRtpTwoByteSizeMax) {
            std::cerr << "invalid packet size, 'kFecExtRtp' is designed to hold packets of size smaller than 32767" << std::endl;
            do_flush();
            return;
        }

        if ((uint16_t)(m_base_rtp_sequence_number + (uint16_t)m_packets.size()) != rtp_header.seq) {
            std::cerr << "sequence number jumps from " << m_base_rtp_sequence_number << " to " << rtp_header.seq << std::endl;
            do_flush();
        }
    }

    if (m_packets.empty()) {
        m_base_rtp_sequence_number = rtp_header.seq;
        m_ssrc                     = rtp_header.ssrc;
    }

    m_packets.push_back({ data, data + data_len });
    if (m_max_packet_len < data_len) {
        m_max_packet_len = data_len;
    }

    assert(!m_encoder);
    if ((int)m_packets.size() == m_config.blocks) {
        encode_group();
    }
}

void
FecEncoderSoftRtp::do_flush() {
    std::cerr << "flush current group with packets: " << m_packets.size() << std::endl;
    if (0 == m_max_packet_len || m_packets.empty()) {
        return;
    }

    /** since we do not allow more encodes after flush, it is safe to modify the red params
     */
    auto red_blocks = (int)(m_config.red_blocks * 1.0 / m_config.blocks * m_packets.size());
    red_blocks = (std::max)(1, red_blocks);

    do_set_red_params((int)m_packets.size(), red_blocks);

    encode_group();
}

void
FecEncoderSoftRtp::encode_group() {
    if (0 == m_max_packet_len || m_packets.empty()) {
        return;
    }

    do_set_block_size(m_max_packet_len);

    assert(!m_encoder);
    do {
        if (!(m_encoder = create_encoder())) {
            std::cerr << "could not create encoder" << std::endl;
            break;
        }

        assert(m_delta_sizes.empty());
        for (auto& packet : m_packets) {
            auto delta = m_active_config.block_size - (int)packet.size();

            assert(delta >= 0);
            assert(delta <= kSoftRtpTwoByteSizeMax);

            if (delta <= kSoftRtpOneByteSizeMax) {
                m_delta_sizes.push_back((uint8_t)delta);
            } else {
                m_delta_sizes.push_back(((uint8_t)(delta >> 8) & 0xff) | 0x80); ///< always big-endian
                m_delta_sizes.push_back((uint8_t)(delta & 0xff));
            }
        }

        bool done = false;
        for (auto& packet : m_packets) {
            assert(!done);
            assert((int)packet.size() <= m_active_config.block_size);

            packet.resize(m_active_config.block_size);
            m_encoder->encode(packet.data(), done);
        }

        assert(done);
    } while (false);

    reset();
}

void
FecEncoderSoftRtp::reset() {
    destroy_encoder();

    m_base_rtp_sequence_number = 0;
    m_ssrc                     = 0;
    m_max_packet_len           = 0;

    m_packets.clear();
    m_delta_sizes.clear();
}

FecHeader*
FecEncoderSoftRtp::make_fec_header(uint16_t sequence, bool red) {
    if (!red) {
        return nullptr;
    }

    auto header = create_fec_header((int)(sizeof(SoftRtp) + m_delta_sizes.size() - 1));
    /** bit 0..1
     */
    header->sig = 0b11; ///< 3
    /** bit 2..3
     */
    header->ver = 0;
    /** bit 4..5
     */
    header->typ = kFecTypeBand == m_type ? 0 : 1;
    /** bit 6..7
     */
    header->mod = fec_mode_to_value(kFecModeSoftRtp);
    /** bit 8
     */
    header->red = red ? 1 : 0;
    /** bit 9..11
     */
    header->sid = 0;
    /** bit 12..15
     */
    header->rsv = 0;
    /** bit 16..31
     */
    header->sequence_number = sequence;

    /** should be called within 'bandfec_encode()' (before 'reset()')
     */
    assert(!m_packets.empty());
    assert(m_packets.size() <= UINT8_MAX);
    assert(m_active_config.block_size > 0);

    auto rtp_ext = (SoftRtp*)header->ext;
    rtp_ext->base_sequence_num = m_base_rtp_sequence_number;
    rtp_ext->delta_size_bytes  = (uint8_t)m_delta_sizes.size();

    std::memcpy(rtp_ext->delta_size, m_delta_sizes.data(), m_delta_sizes.size());
    return header;
}
