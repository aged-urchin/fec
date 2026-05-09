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
FecEncoderSoftRtp::do_set_max_reorder_depth(int packets) {
    if (packets <= 0) {
        std::cerr << "invalid arguments" << std::endl;
        return;
    }

    m_max_reorder_depth = packets;
}

void
FecEncoderSoftRtp::do_encode(const uint8_t* data, int data_len) {
    if (data_len > kSoftRtpTwoByteSizeMax) {
        std::cerr << "invalid packet size, could not handle packets of size larger than 32767" << std::endl;
        return;
    }

    RtpHeader rtp_header;
    if (!parse_rtp_buffer(data, data_len, rtp_header)) {
        std::cerr << "invalid rtp packet" << std::endl;
        return;
    }

    ///< std::cerr << "encode seq: " << rtp_header.seq << ", len: " << data_len << std::endl;
    if (m_ssrc >= 0 && m_ssrc != rtp_header.ssrc) {
        std::cerr << "ssrc differs, was: " << m_ssrc << ", new: " << rtp_header.ssrc << std::endl;
        return;
    }

    if (m_ssrc < 0) {
        std::cerr << "setting ssrc to " << rtp_header.ssrc << std::endl;
        m_ssrc = rtp_header.ssrc;
    }

    /** all sequence-based reorder bookkeeping is done in unwrapped 64-bit space so a 16-bit RTP wrap-around does not break ordering decisions.
     */
    auto unwrapped_seq = m_seq_unwrapper.unwrap(rtp_header.seq);
    /** this packet is older than the earliest sequence we are still willing to feed into FEC.
     *  (e.g. it has either already been encoded or was abandoned after reorder overflow)
     */
    if (m_min_acceptable_rtp_sequence >= 0 && unwrapped_seq < m_min_acceptable_rtp_sequence) {
        std::cerr << "drop a late rtp packet(seq: " << rtp_header.seq << "), acceptable: " << m_min_acceptable_rtp_sequence << std::endl;
        return;
    }

    /** ignore duplicates while the packet is still parked in the reorder buffer.
     */
    if (m_seen_rtp_sequences.count(unwrapped_seq) > 0) {
        return;
    }

    /** cache the newly arrived packet first,
     *  only consecutive packets starting from m_expected_rtp_sequence can be drained into the current FEC group.
     */
    m_seen_rtp_sequences.insert(unwrapped_seq);
    m_pending_packets[unwrapped_seq] = { data, data + data_len };

    if (m_latest_rtp_sequence < unwrapped_seq) {
        m_latest_rtp_sequence = unwrapped_seq;
    }

    if (m_expected_rtp_sequence < 0 && !m_pending_packets.empty()) {
        m_expected_rtp_sequence = m_pending_packets.begin()->first;
    }

    /** try to move as many now-consecutive packets as possible from the reorder buffer into the current FEC group.
     */
    drain_pending_packets();

    /** we still have a hole at m_expected_rtp_sequence, but packets far enough
     *  ahead have already arrived. Give up waiting for that missing range and restart from the earliest pending packet.
     */
    std::ostringstream os;
    while (is_reorder_window_full()) {
        if (os.str().empty()) {
            for (auto& packet : m_pending_packets) {
                os << packet.first << " ";
            }
        }
        handle_reorder_window_full();
    }

    if (!os.str().empty()) {
        os << " ===> ";
        for (auto& packet : m_pending_packets) {
            os << packet.first << " ";
        }
        std::cerr << "reorder windows exceeded: " << os.str() << std::endl;
    }
}

void
FecEncoderSoftRtp::do_flush() {
    /** stream-ending flush does not wait for missing packets any more: drain each remaining pending run starting from its earliest available packet.
     */
    do {
        flush_ordered_group();
        if (m_pending_packets.empty()) {
            break;
        }

        m_expected_rtp_sequence = m_pending_packets.begin()->first;
        drain_pending_packets();
    } while (true);
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

bool
FecEncoderSoftRtp::append_packet_to_group(const uint8_t* data, int data_len, uint16_t rtp_seq) {
    /** the first packet appended after a flush becomes the base RTP sequence advertised in the red packet headers for this FEC group.
     */
    if (m_packets.empty()) {
        m_base_rtp_sequence_number = rtp_seq;
    }

    m_packets.push_back({ data, data + data_len });
    if (m_max_packet_len < data_len) {
        m_max_packet_len = data_len;
    }

    assert(!m_encoder);
    if ((int)m_packets.size() == m_config.blocks) {
        encode_group();
        return true;
    }

    return false;
}

void
FecEncoderSoftRtp::flush_ordered_group() {
    std::cerr << "flush current group with packets: " << m_packets.size() << std::endl;
    if (0 == m_max_packet_len || m_packets.empty()) {
        return;
    }

    /** emit a shortened group with a proportionally reduced redundancy count.
     *  the original configured group size is restored right after encoding so future full groups still use the requested protection level.
     */
    auto red_blocks = (int)(m_config.red_blocks * 1.0 / m_config.blocks * m_packets.size());
    red_blocks = (std::max)(1, red_blocks);

    auto blocks_in_group     = m_config.blocks;
    auto red_blocks_in_group = m_config.red_blocks;
    do_set_red_params((int)m_packets.size(), red_blocks);

    encode_group();
    /** restore the original params
     */
    do_set_red_params(blocks_in_group, red_blocks_in_group);
}

void
FecEncoderSoftRtp::drain_pending_packets() {
    /** consume a consecutive run [expected, expected + n) from the reorder buffer.
     *  the moment we hit a gap, draining stops and the missing sequence keeps blocking later packets until either it arrives or the reorder window overflows.
     */
    while (!m_pending_packets.empty() && m_expected_rtp_sequence >= 0) {
        auto itr = m_pending_packets.find(m_expected_rtp_sequence);
        if (itr == m_pending_packets.end()) {
            break;
        }

        auto packet  = std::move(itr->second);
        auto rtp_seq = (uint16_t)m_expected_rtp_sequence;

        m_pending_packets.erase(itr);
        m_seen_rtp_sequences.erase(m_expected_rtp_sequence);

        /** anything older than the next expected sequence is no longer useful to the encoder.
         */
        m_min_acceptable_rtp_sequence = ++m_expected_rtp_sequence;

        auto group_encoded = append_packet_to_group(packet.data(), (int)packet.size(), rtp_seq);
        if (group_encoded) {
            /** group boundaries do not reset m_expected_rtp_sequence.
             *  we still wait for the earliest missing packet in the overall input stream so a slightly late packet can join the next FEC group.
             */
            if (!m_pending_packets.empty()) {
                m_latest_rtp_sequence = m_pending_packets.rbegin()->first;
            } else {
                m_latest_rtp_sequence = -1;
            }
        }
    }
}

bool
FecEncoderSoftRtp::is_reorder_window_full() {
    return !m_pending_packets.empty() && m_expected_rtp_sequence >= 0 && m_latest_rtp_sequence - m_expected_rtp_sequence >= m_max_reorder_depth;;
}

void
FecEncoderSoftRtp::handle_reorder_window_full() {
    assert(!m_pending_packets.empty());
    std::cerr << "reorder window exceeded, flush current ordered group, expected: "
                << m_expected_rtp_sequence << ", latest: " << m_latest_rtp_sequence << std::endl;

    flush_ordered_group();

    /** at this point we explicitly abandon the missing range before the earliest pending packet and restart the protected run from there.
     */
    m_expected_rtp_sequence = m_pending_packets.begin()->first;
    m_latest_rtp_sequence   = m_pending_packets.rbegin()->first;

    drain_pending_packets();
}

void
FecEncoderSoftRtp::reset() {
    destroy_encoder();

    m_base_rtp_sequence_number = 0;
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
