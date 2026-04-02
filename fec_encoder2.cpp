#include "fec_encoder2.h"
#include "bandfec.h"
#include "utils.h"

#include <iostream>
#include <cassert>

FecEncoder2::FecEncoder2(IFecEncoderObserver* observer) :
FecEncoderBase(observer) {

}

bool
FecEncoder2::set_red_params(int blocks_in_group, int red_blocks_in_group) {
    /** 2: kRtpFecExtTwoByteSizeMax
     */
    if (blocks_in_group > UINT8_MAX / 2) {
        std::cerr << "invalid arguments, blocks(" << blocks_in_group << ") too large for 'kFecExtRtp'" << std::endl;
        return false;
    }

    return FecEncoderBase::set_red_params(blocks_in_group, red_blocks_in_group);
}

void
FecEncoder2::do_encode(const uint8_t* data, int data_len) {
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

        if (data_len > kRtpFecExtTwoByteSizeMax) {
            std::cerr << "invalid packet size, 'kFecExtRtp' is designed to hold packets of size smaller than 32767" << std::endl;
            do_flush();
            return;
        }

        if (m_base_rtp_sequence_number + m_packets.size() != rtp_header.seq) {
            std::cerr << "sequence number jumps from " << m_base_rtp_sequence_number << "to " << rtp_header.seq << std::endl;
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
    if (m_packets.size() == m_config.blocks) {
        encode_group();
    }
}

void
FecEncoder2::do_flush() {
    std::cerr << "flush current group with packets: " << m_packets.size() << std::endl;
    if (0 == m_max_packet_len || m_packets.empty()) {
        return;
    }

    /** since we do not allow more encodes after flush, it is safe to modify the red params
     */
    do_set_red_params(m_packets.size(), (int)(m_config.red_blocks * 1.0 / m_config.blocks * m_packets.size()));

    encode_group();
}

void
FecEncoder2::encode_group() {
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
            auto delta = m_active_config.block_size - packet.size();

            assert(delta >= 0);
            assert(delta <= kRtpFecExtTwoByteSizeMax);

            if (delta <= kRtpFecExtOneByteSizeMax) {
                m_delta_sizes.push_back(delta);
            } else {
                m_delta_sizes.push_back(((uint8_t)(delta >> 8) & 0xff) | 0x80); ///< always big-endian
                m_delta_sizes.push_back((uint8_t)(delta & 0xff));
            }
        }

        bool done = false;
        for (auto& packet : m_packets) {
            assert(!done);
            assert(packet.size() <= m_active_config.block_size);

            packet.resize(m_active_config.block_size);
            bandfec_encode(m_encoder, (int32_t*)packet.data(), done);
        }

        assert(done);
    } while (false);

    reset();
}

void
FecEncoder2::reset() {
    destroy_bandfec_encoder(m_encoder);
    m_encoder = nullptr;

    m_base_rtp_sequence_number = 0;
    m_ssrc                     = 0;
    m_max_packet_len           = 0;

    m_packets.clear();
    m_delta_sizes.clear();
}

FecHeader*
FecEncoder2::make_fec_header(uint16_t sequence, bool red) {
    if (!red) {
        //return nullptr;
    }

    auto header = create_fec_header(sizeof(RtpFecExt) + m_delta_sizes.size() - 1);

    header->sig = 0b11; ///< 3
    header->typ = kFecExtRtp;
    header->sid = 0;
    header->red = red;

    header->reserved        = 0;
    header->sequence_number = sequence;

    /** should be called within 'bandfec_encode()' (before 'reset()')
     */
    assert(!m_packets.empty());
    assert(m_packets.size() <= UINT8_MAX);
    assert(m_active_config.block_size > 0);

    auto rtp_ext = (RtpFecExt*)header->ext;
    rtp_ext->base_sequence_num = m_base_rtp_sequence_number;
    rtp_ext->delta_size_bytes  = m_delta_sizes.size();

    std::memcpy(rtp_ext->delta_size, m_delta_sizes.data(), m_delta_sizes.size());
    return header;
}
