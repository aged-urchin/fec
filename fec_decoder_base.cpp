#include "fec_decoder_base.h"
#include "fec_packet.h"
#include "bandfec.h"

#include <iostream>
#include <cassert>

void
on_fec_receive(BandFecDecoder* f, int64_t position, void* buf, int len, int64_t user_data1, int64_t user_data2) {
    auto decoder  = (FecDecoderBase*)user_data1;
    auto sequence = (uint32_t)user_data2;

    if (position % len) {
        std::cerr << "invalid position " << position << ", block size is " << len << std::endl;
    }

    decoder->on_new_block(sequence, (int32_t)position / len, (const uint8_t*)buf, len);
}

FecDecoderBase::FecDecoderBase(IFecDecoderObserver* observer) :
m_observer(observer) {

}

FecDecoderBase::~FecDecoderBase() {
    assert(m_seq_decoders.empty());
}

void
FecDecoderBase::set_max_forward_packets(int packets) {
    m_max_forward_packets = packets;
    std::cerr << "max forward packets changed to " << packets << std::endl;
}

void
FecDecoderBase::decode(const uint8_t* data, int len) {
    auto packet = FecPacket::parse_from_buffer(data, len);
    if (!packet) {
        return;
    }

    auto header = packet->get_header();
    maybe_remove_outdated_decoders(header->sequence_number);

    if (0 == m_seq_decoders.count(header->sequence_number)) {
        auto decoder = new Decoder;
        decoder->decoder = create_bandfec_decoder(&on_fec_receive, (int64_t)this, header->sequence_number);

        m_seq_decoders[header->sequence_number] = decoder;

        BandFecHeaderType bandfec_header;
        bandfec_parse_block((void*)packet->get_payload(), packet->get_payload_size(), bandfec_header);

        on_sequence_start(header->sequence_number, header, &bandfec_header);
    }

    decode_fec_block(header->sequence_number, packet->get_payload(), packet->get_payload_size());
    packet->release();
}

void
FecDecoderBase::loss_stats(PacketLossStats& stats) {
    stats.lossrate              = -1;
    stats.discontinuity_groups  = -1;
    stats.lost_packets          = -1;
    stats.recovered_packets     = -1;
    stats.late_packets          = -1;
    stats.num_distributions     = -1;
    stats.distributions         = nullptr;
}

void
FecDecoderBase::maybe_remove_outdated_decoders(int32_t reset_sequence_number) {
    std::vector<uint16_t> outdated_decoders;
    for (auto& decoder : m_seq_decoders) {
        if ((int32_t)decoder.first == reset_sequence_number) {
            decoder.second->max_consecutive_next_group_packets = 0;
        } else if (++decoder.second->max_consecutive_next_group_packets > m_max_forward_packets) {
            outdated_decoders.push_back(decoder.first);
        }
    }

    for (auto& decoder : outdated_decoders) {
        remove_decoder(decoder);
    }
}

void
FecDecoderBase::decode_fec_block(uint16_t sequence_number, const void* data, int len) {
    auto decoder = m_seq_decoders[sequence_number];
    /** sequential delivery is unnecessary(the decoder accepts out-of-order packets)
     */
    bandfec_decode(decoder->decoder, (void*)data, len);
}

void
FecDecoderBase::send_frame(uint16_t sequence_number, uint16_t frame_number, const uint8_t* data, int data_len) {
    ///< std::cerr << "seq: " << sequence_number << ", frame: " << frame_number << std::endl;
    m_observer->on_decoder_output(this, sequence_number, frame_number, data, data_len);
}

void
FecDecoderBase::remove_decoder(uint16_t sequence) {
    if (0 == m_seq_decoders.count(sequence)) {
        return;
    }

    auto decoder = m_seq_decoders[sequence];

    flush_bandfec_decoder(decoder->decoder);
    destroy_bandfec_decoder(decoder->decoder);

    delete decoder;
    m_seq_decoders.erase(sequence);

    on_sequence_end(sequence);
}

void
FecDecoderBase::destroy_decoders() {
    while (!m_seq_decoders.empty()) {
        auto itr = m_seq_decoders.begin();
        remove_decoder(itr->first);
    }
}
