#include "fec_decoder_softrtp.h"
#include "./utils/utils.h"
#include "./utils/timeutils.h"

#include <iostream>
#include <cassert>

FecDecoderSoftRtp::FecGroup::FecGroup(uint16_t sequence, const FecHeaderInfo* info, SoftRtp* rtp_ext) :
sequence(sequence) {
    assert(info);
    assert(info->pack);
    assert(rtp_ext);

    if (!info || !rtp_ext || rtp_ext->delta_size_bytes <= 0) {
        std::cerr << "invalid arguments" << std::endl;
        return;
    }

    header_info = *info;

    auto ptr             = (uint8_t*)rtp_ext->delta_size;
    auto remaining_bytes = (int)rtp_ext->delta_size_bytes;

    while (remaining_bytes > 0) {
        auto is_one_byte_len = 0 == (*ptr & 0x80);
        uint16_t delta_size = 0;

        if (is_one_byte_len) {
            /** one-byte delta size
             */
            delta_size = *ptr++;
        } else {
            /** two-byte delta size (big-endian)
             */
            delta_size |= ((uint8_t)(*ptr++ & 0x7f) << 8);
            delta_size |= *ptr++;
        }

        rtp_packet_sizes[rtp_ext->base_sequence_num++] = header_info.s - delta_size;

        remaining_bytes -= (is_one_byte_len ? 1 : 2);
    }
}

FecDecoderSoftRtp::FecGroup::~FecGroup() {

}

FecDecoderSoftRtp::FecDecoderSoftRtp(FecType type, IFecDecoderObserver* observer) :
FecDecoderBase(type, kFecModeSoftRtp, observer) {

}

FecDecoderSoftRtp::~FecDecoderSoftRtp() {

}

void
FecDecoderSoftRtp::destroy(PacketLossStats* stats) {
    destroy_decoders(stats);
}

void
FecDecoderSoftRtp::decode(const uint8_t* data, int len) {
    if (!is_rtp(data, len)) {
        /** a fec packet (with 'FecHeader::red' == 1)
         */
        FecDecoderBase::decode(data, len);
        return;
    }
    /** a rtp packet
     */
    RtpHeader rtp_header;
    if (!parse_rtp_buffer(data, len, rtp_header)) {
        std::cerr << "decoder outputs an invalid rtp packet" << std::endl;
        return;
    }

    if (m_ssrc < 0) {
        m_ssrc = rtp_header.ssrc;
        std::cerr << "set ssrc to " << rtp_header.ssrc << " on receiving the very first rtp packet" << std::endl;
    } else if (m_ssrc != rtp_header.ssrc) {
        /** fecencoder2/fecdecoder2 are supposed to be used to a single media stream
         */
        std::cerr << "invalid rtp packet, ssrc does not match!" << std::endl;
        return;
    }

    /** a fecgroup is unlikely to be larger than 65535
     */
    assert(0 == m_rtp_packets.count(rtp_header.seq));
    /** decode this packet if the first red packet of this group has arrived 
     */
    if (maybe_decode_rtp_packet(rtp_header.seq, data, len)) {
        return;
    }

    auto now_ms = Time::clocktime();
    /** increase 'Decoder::no_packets_cnt' of all existing sequence decoders, for this is a rtp packet of a new sequence
     */
    purge_expired_data(now_ms);

    /** cache all rtp packets before the first red packet is recieved
     */
    StoredRtpPacket stored_packet;
    stored_packet.arrivetime_ms = now_ms;
    stored_packet.packet        = { data, data + len };

    m_rtp_packets[rtp_header.seq] = stored_packet;
}

bool
FecDecoderSoftRtp::is_rtp(const uint8_t* data, size_t len) {
    /** we could tell the difference between a rtp packet and a fec packet from the beginning two bits
     */
    return len >= 12 && 0x80 == (data[0] & 0xC0);
}

void
FecDecoderSoftRtp::purge_expired_data(int64_t now_ms) {
    auto lifetime_ms = get_max_packet_lifetime();
    for (auto itr = m_rtp_packets.begin(); itr != m_rtp_packets.end(); ) {
        auto rtp_packet = *itr;
        if (now_ms - itr->second.arrivetime_ms >= lifetime_ms) {
            std::cerr << "purge overdue rtp packet with sequence number: " << itr->first << std::endl;
            itr = m_rtp_packets.erase(itr);
        } else {
            ++itr;
        }
    }

    maybe_remove_outdated_decoders(now_ms);
}

bool
FecDecoderSoftRtp::maybe_decode_rtp_packet(uint16_t rtp_sequence, const uint8_t* rtp_packet_data, int rtp_packet_len) {
    FecGroup* group = nullptr;
    for (auto& fec_group : m_fec_groups) {
        if (fec_group.second->rtp_packet_sizes.count(rtp_sequence) > 0) {
            /** we have header from a decoded red fec packet, use it to construct a fec block.
             */
            group = fec_group.second;
            break;
        }
    }

    if (!group) {
        return false;
    }

    decode_rtp(rtp_sequence, group, rtp_packet_data, rtp_packet_len);
    return true;
}

void
FecDecoderSoftRtp::decode_rtp(uint16_t rtp_sequence, const FecGroup* fec_group, const void* data, int len) {
    /** construct a fec block and send it to the decoder
     */
    auto header   = fec_group->header_info;
    auto base_seq = fec_group->rtp_packet_sizes.begin()->first;

    assert(rtp_sequence >= base_seq);
    assert(rtp_sequence <= base_seq + header.n);
    /** 'i' takes values from 0 to 'n' + 'k' - 1
     */
    header.i = rtp_sequence - base_seq;

    auto fec_block = header.pack(header);
    fec_block.insert(fec_block.end(), (char*)data, (char*)data + len);

    assert(len <= fec_group->header_info.s);
    /** align(padding with 0) on 's'
     */
    fec_block.resize(fec_group->header_info.header_size + fec_group->header_info.s);

    decode_fec_block(fec_group->sequence, &header, fec_block.data(), (int)fec_block.size(), false);
}

void
FecDecoderSoftRtp::on_sequence_start(uint16_t sequence, const FecHeader* header, const FecHeaderInfo* info) {
    std::cerr << "sequence " << sequence << " starts" << std::endl;

    assert(0 == m_fec_groups.count(sequence));
    assert(kFecModeSoftRtp == fec_mode_from_value(header->mod));
    /** a new group should start with a red packet
     *  which has a 'i' ranging from 'n' to 'n + k'
     */
    assert(1 == header->red);
    assert(info->i >= info->n);
    assert(info->i < (uint32_t)(info->n + info->k));
    
    auto rtp_ext = (SoftRtp*)header->ext;
    m_fec_groups[sequence] = new(std::nothrow) FecGroup(sequence, info, rtp_ext);

    /** check the cached rtp packets upon receiving the first red packet, and start decoding them if they belong to this group
     */
    for (auto itr = m_rtp_packets.begin(); itr != m_rtp_packets.end(); ) {
        auto rtp_packet = *itr;
        if (maybe_decode_rtp_packet(rtp_packet.first, rtp_packet.second.packet.data(), (int)rtp_packet.second.packet.size())) {
            itr = m_rtp_packets.erase(itr);
        } else {
            ++itr;
        }
    }
}

void
FecDecoderSoftRtp::on_sequence_end(uint16_t sequence) {
    std::cerr << "sequence " << sequence << " ends" << std::endl;
    if (0 == m_fec_groups.count(sequence)) {
        std::cerr << "no such a sequence: " << sequence << std::endl;
        return;
    }

    auto group = m_fec_groups[sequence];
    delete group;

    m_fec_groups.erase(sequence);
}

void
FecDecoderSoftRtp::on_new_block(uint16_t sequence_number, int32_t pos, const uint8_t* data, int len, bool recovered) {
    assert(m_fec_groups.count(sequence_number) != 0);
    if (0 == m_fec_groups.count(sequence_number)) {
        std::cerr << "no sequence number(" << sequence_number << ") found" << std::endl;
        return;
    }

    if (!recovered) {
        /** we only output recovered rtp packets
         */
        return;
    }

    RtpHeader rtp_header;
    if (!parse_rtp_buffer(data, len, rtp_header)) {
        std::cerr << "decoder outputs an invalid rtp packet" << std::endl;
        return;
    }

    assert(m_fec_groups[sequence_number]->rtp_packet_sizes.count(rtp_header.seq) != 0);
    assert(m_fec_groups[sequence_number]->rtp_packet_sizes[rtp_header.seq] <= len);
    if (0 == m_fec_groups[sequence_number]->rtp_packet_sizes.count(rtp_header.seq) ||
        m_fec_groups[sequence_number]->rtp_packet_sizes[rtp_header.seq] > len) {
        std::cerr << "invalid rtp sequence number: " << rtp_header.seq << std::endl;
        return;
    }

    if (m_ssrc < 0) {
        m_ssrc = rtp_header.ssrc;
        std::cerr << "set ssrc to " << rtp_header.ssrc << " on decoding the very first packet" << std::endl;
    } else if (m_ssrc != rtp_header.ssrc) {
        std::cerr << "invalid rtp packet, ssrc does not match!" << std::endl;
        return;
    }

    assert(pos >= 0);
    assert(pos < m_fec_groups[sequence_number]->header_info.n);

    /** use 'rtp sequence number' as the 2nd param?
     */
    auto rtp_packet_size = m_fec_groups[sequence_number]->rtp_packet_sizes[rtp_header.seq];
    send_frame(sequence_number, rtp_header.seq, data, rtp_packet_size);
}
