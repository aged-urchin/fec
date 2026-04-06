#include "fec_decoder2.h"
#include "bandfec.h"
#include "utils.h"

#include <iostream>
#include <cassert>

FecDecoder2::FecGroup::FecGroup(uint16_t sequence, const BandFecHeaderType* header, RtpFecExt* rtp_ext) :
sequence(sequence),
bandfec_header(nullptr) {
    assert(header);
    assert(rtp_ext);
    if (!header || !rtp_ext || rtp_ext->delta_size_bytes <= 0) {
        std::cerr << "invalid arguments" << std::endl;
        return;
    }

    bandfec_header  = new BandFecHeaderType;
    *bandfec_header = *header;

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

        rtp_packet_sizes[rtp_ext->base_sequence_num++] = bandfec_header->s - delta_size;

        remaining_bytes -= (is_one_byte_len ? 1 : 2);
    }
}

FecDecoder2::FecGroup::~FecGroup() {
    delete bandfec_header;
}

FecDecoder2::FecDecoder2(IFecDecoderObserver* observer) :
FecDecoderBase(observer) {

}

FecDecoder2::~FecDecoder2() {
    destroy_decoders();
}

void
FecDecoder2::decode(const uint8_t* data, int len) {
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

    /** increase 'Decoder::no_packets_cnt' of all existing sequence decoders, for this is a rtp packet of a new sequence
     */
    maybe_remove_outdated_decoders();
    /** cache all rtp packets before the first red packet is recieved
     */
    m_rtp_packets[rtp_header.seq] = { data, data + len };
}

bool
FecDecoder2::is_rtp(const uint8_t* data, size_t len) {
    /** we could tell the difference between a rtp packet and a fec packet from the beginning two bits
     */
    return len >= 12 && 0x80 == (data[0] & 0xC0);
}

bool
FecDecoder2::maybe_decode_rtp_packet(uint16_t rtp_sequence, const uint8_t* rtp_packet_data, int rtp_packet_len) {
    FecGroup* group = nullptr;
    for (auto& fec_group : m_fec_groups) {
        if (fec_group.second->rtp_packet_sizes.count(rtp_sequence) > 0) {
            /** we have 'BandFecHeaderType' from a decoded red fec packet, use it to construct a fec block.
             */
            group = fec_group.second;
            break;
        }
    }

    if (!group) {
        return false;
    }

    group->available_rtp_packets.push_back(rtp_sequence);
    decode_rtp(rtp_sequence, group, rtp_packet_data, rtp_packet_len);
}

void
FecDecoder2::decode_rtp(uint16_t rtp_sequence, const FecGroup* fec_group, const void* data, int len) {
    /** construct a fec block and send it to the BandFecDecoder
     */
    auto header   = *fec_group->bandfec_header;
    auto base_seq = fec_group->rtp_packet_sizes.begin()->first;

    assert(rtp_sequence >= base_seq);
    assert(rtp_sequence <= base_seq + header.n);
    /** 'BandFecHeaderType::i' takes values from 0 to 'BandFecHeaderType::n' + 'BandFecHeaderType::k' - 1
     */
    header.i = rtp_sequence - base_seq;
    /** convert 'BandFecHeaderType' from host to network
     */
    header.s = UINT16_TO_BE(header.s);
    header.n = UINT16_TO_BE(header.n);
    header.k = UINT16_TO_BE(header.k);
    header.i = UINT32_TO_BE(header.i);

    std::vector<uint8_t> fec_block;
    fec_block.insert(fec_block.end(), (char*)&header, (char*)&header + sizeof(header));
    fec_block.insert(fec_block.end(), (char*)data, (char*)data + len);

    assert(len <= fec_group->bandfec_header->s);
    /** align(padding with 0) on 'BandFecHeaderType::s'
     */
    fec_block.resize(sizeof(BandFecHeaderType) + fec_group->bandfec_header->s);

    decode_fec_block(fec_group->sequence, fec_block.data(), fec_block.size());
}

void
FecDecoder2::on_sequence_start(uint16_t sequence, const FecHeader* header, const BandFecHeaderType* bandfec_header) {
    std::cerr << "sequence " << sequence << " starts" << std::endl;

    assert(0 == m_fec_groups.count(sequence));
    assert(1 == header->typ);
    /** a new group should start with a red packet
     *  which has a 'BandFecHeaderType::i' ranging from 'BandFecHeaderType::n' to 'BandFecHeaderType::n + BandFecHeaderType::k'
     */
    assert(1 == header->red);
    assert(bandfec_header->i >= bandfec_header->n);
    assert(bandfec_header->i < bandfec_header->n + bandfec_header->k);
    
    auto rtp_ext = (RtpFecExt*)header->ext;
    m_fec_groups[sequence] = new FecGroup(sequence, bandfec_header, rtp_ext);

    /** check the cached rtp packets upon receiving the first red packet, and start decoding them if they belong to this group
     */
    for (auto itr = m_rtp_packets.begin(); itr != m_rtp_packets.end(); ) {
        auto rtp_packet = *itr;
        if (maybe_decode_rtp_packet(rtp_packet.first, rtp_packet.second.data(), rtp_packet.second.size())) {
            itr = m_rtp_packets.erase(itr);
        } else {
            ++itr;
        }
    }
}

void
FecDecoder2::on_sequence_end(uint16_t sequence) {
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
FecDecoder2::on_new_block(uint16_t sequence_number, int32_t pos, const uint8_t* data, int len) {
    assert(m_fec_groups.count(sequence_number) != 0);
    if (0 == m_fec_groups.count(sequence_number)) {
        std::cerr << "no sequence number(" << sequence_number << ") found" << std::endl;
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

    auto avail_rtp = m_fec_groups[sequence_number]->available_rtp_packets;
    if (avail_rtp.end() != std::find(avail_rtp.begin(), avail_rtp.end(), rtp_header.seq)) {
        /** we only output missing rtp packets
         */
        return;
    }

    assert(pos >= 0);
    assert(pos < m_fec_groups[sequence_number]->bandfec_header->n);

    /** use 'rtp sequence number' as the 2nd param?
     */
    auto rtp_packet_size = m_fec_groups[sequence_number]->rtp_packet_sizes[rtp_header.seq];
    send_frame(sequence_number, rtp_header.seq, data, rtp_packet_size);

    ///< std::cerr << "decode seq: " << rtp_header.seq << ", len: " << rtp_packet_size << std::endl;
}
