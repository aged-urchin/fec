#include "bandfec_decoder.h"
#include "bandfec.h"
#include "fec_packet.h"

#include <iostream>

void
on_fec_receive(FecDecoder* f, int64_t position, void* buf, int len, int64_t user_data1, int64_t user_data2) {
    auto decoder  = (BandFecDecoder*)user_data1;
    auto sequence = (uint32_t)user_data2;

    if (position % len) {
        printf("error\n");
    }

    decoder->on_new_block(sequence, (int32_t)position / len, (const uint8_t*)buf, len);
}

BandFecDecoder::ReconstructedFrame::ReconstructedFrame(int size) {
    if (size > 0) {
        data.resize(size);
    }
}

bool
BandFecDecoder::ReconstructedFrame::ready() {
    int total_written = 0;
    for (const auto& slot : slots) {
        total_written += slot.second;
    }

    return !data.empty() && total_written == data.size();
}

bool
BandFecDecoder::ReconstructedFrame::push_fragment(int offset, const uint8_t* fragment_data, int size) {
    if (data.empty()) {
        std::cerr << "invalid operation" << std::endl;
        return false;
    }

    if (offset < 0 || size <= 0 || !fragment_data || offset + size > data.size()) {
        std::cerr << "invalid arguments" << std::endl;
        return false;
    }

    /** determine if slot is available
     */
    int fragment_end = offset + size;
    for (const auto& slot : slots) {
        int slot_start = slot.first;
        int slot_end = slot_start + slot.second;

        if (offset < slot_end && fragment_end > slot_start) {
            std::cerr << "slot conflict" << std::endl;
            return false;
        }
    }

    std::copy(fragment_data, fragment_data + size, data.begin() + offset);
    slots[offset] = size;

    return true;
}

BandFecDecoder::BandFecDecoder(IBandFecDecoderObserver* observer) :
m_observer(observer) {

}

BandFecDecoder::~BandFecDecoder() {
    while (!m_pending_frames.empty()) {
        auto itr = m_pending_frames.begin();
        delete itr->second;

        m_pending_frames.erase(itr);
    }

    while (!m_seq_decoders.empty()) {
        auto itr = m_seq_decoders.begin();
        remove_decoder(itr->first);
    }
}

void
BandFecDecoder::decode(const uint8_t* data, int len) {
    auto packet = FecPacket::parse_from_buffer(data, len);
    if (!packet) {
        return;
    }

    FecHeader header;
    packet->get_header(header);

    std::vector<uint16_t> outdated_decoders;
    for (auto& decoder : m_seq_decoders) {
        if (decoder.first == header.sequence_number) {
            decoder.second->no_packets_cnt = 0;
        } else if (++decoder.second->no_packets_cnt >= kDeathCounterOnNoData) {
            outdated_decoders.push_back(decoder.first);
        }
    }

    for (auto& decoder : outdated_decoders) {
        remove_decoder(decoder);
    }

    if (0 == m_seq_decoders.count(header.sequence_number)) {
        auto decoder = new Decoder;
        decoder->decoder = create_fec_decoder(&on_fec_receive, (int64_t)this, 0);

        m_seq_decoders[header.sequence_number] = decoder;
    }

    auto decoder = m_seq_decoders[header.sequence_number];
    fec_decode(decoder->decoder, (void*)packet->get_payload(), packet->get_payload_size());
}

void
BandFecDecoder::remove_decoder(uint16_t sequence) {
    if (0 == m_seq_decoders.count(sequence)) {
        return;
    }

    auto decoder = m_seq_decoders[sequence];

    flush_fec_decoder(decoder->decoder);
    destroy_fec_decoder(decoder->decoder);

    delete decoder;
    m_seq_decoders.erase(sequence);
}

void
BandFecDecoder::on_new_block(uint16_t sequence, int32_t pos, const uint8_t* data, int len) {
    uint8_t* remaining_data = (uint8_t*)data;
    int      remaining_len  = len;

    while (remaining_len > sizeof(FecFragmentHeader)) {
        auto frag_header = (FecFragmentHeader*)remaining_data;
        auto header      = frag_header->to_host();

        if (header.is_empty()) {
            std::cerr << "empty fragment, skip this block" << std::endl;
            return;
        }

        remaining_data += sizeof(FecFragmentHeader);
        remaining_len  -= sizeof(FecFragmentHeader);

        if (remaining_len < header.frag_size) {
            std::cerr << "invalid fragment" << std::endl;
            return;
        }

        if (0 == m_pending_frames.count(header.frame_number)) {
            m_pending_frames[header.frame_number] = new ReconstructedFrame(header.frame_size);
        }

        auto frame = m_pending_frames[header.frame_number];
        auto ret   = frame->push_fragment(header.frag_offset, remaining_data, header.frag_size);
        if (!ret) {
            return;
        }

        remaining_data += header.frag_size;
        remaining_len  -= header.frag_size;

        if (frame->ready()) {
            m_observer->on_decoder_output(this, sequence, header.frame_number, frame->data.data(), frame->data.size());

            m_pending_frames.erase(header.frame_number);
            delete frame;
        }
    }

    if (remaining_len != 0) {
        std::cerr << "invalid argument" << std::endl;
    }
}
