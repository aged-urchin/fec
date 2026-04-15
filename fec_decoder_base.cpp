#include "fec_decoder_base.h"
#include "fec_packet.h"
#include "bandfec.h"
#include "./utils/number_unwrapper.h"
#include "./utils/timeutils.h"

#include <iostream>
#include <sstream>
#include <cassert>
#include <set>

void
on_fec_receive(BandFecDecoder* f, int64_t position, void* buf, int len, int64_t user_data1, int64_t user_data2) {
    auto decoder  = (FecDecoderBase*)user_data1;
    auto sequence = (uint32_t)user_data2;

    if (position % len) {
        std::cerr << "invalid position " << position << ", block size is " << len << std::endl;
    }

    decoder->on_block_decoded(sequence, (int32_t)position / len, (const uint8_t*)buf, len);
}

FecDecoderBase::FecDecoderBase(IFecDecoderObserver* observer) :
m_observer(observer) {

}

void
FecDecoderBase::Decoder::collect_stats(uint16_t sequence, Stats& stats) {
    auto num_data_packets      = data_packets.size();
    auto num_red_packets       = red_packets.size();
    auto num_recovered_packets = recovered_packets;

    assert(n >= num_data_packets); ///<!!! no retransmission of fec blocks
    assert(n >= num_data_packets + num_recovered_packets);

    std::set<int32_t> packets;
    packets.insert(data_packets.begin(), data_packets.end());
    packets.insert(red_packets.begin(), red_packets.end());

    assert(!packets.empty());
    /** update stats
     */
    int32_t prev = kFirstBlockIndex;
    for (auto it = packets.begin(); it != packets.end(); ++it) {
        auto curr = *it;
        auto gap  = curr - prev - 1;

        if (gap > 0 && gap <= kMaxContLossCount) {
            ++stats.loss_distribution[gap];
        }

        prev = curr;
    }

    stats.expected_data_packets = n;
    stats.received_data_packets = num_data_packets;
    stats.recovered_packets     = num_recovered_packets;
    /** logs
     */
    std::ostringstream data_str, red_str;

    data_str << "recv: " << num_data_packets << " (";
    for (auto& id : data_packets) {
        data_str << id << " ";
    }
    data_str << ")";

    red_str << "red: " << num_red_packets << " (";
    for (auto& id : red_packets) {
        red_str << id << " ";
    }
    red_str << ")";

    auto intact = n == (int)(num_data_packets + num_recovered_packets);
    assert(!intact || ((int)(num_data_packets + num_red_packets) >= n));

    if (!intact && (int)(num_data_packets + num_red_packets) >= n) {
        std::cerr << "maybe RS code could recover this group(" << sequence << ")!" << std::endl;
    }

    std::cerr << "finish sequence: " << sequence << "(n: " << n << ", k: " << k << ")"
              << ", " << data_str.str() << ", " << red_str.str()
              << ", recovered: " << num_recovered_packets << ", lost: " << n - num_data_packets - num_recovered_packets
              << ", intact: " << std::boolalpha << intact << std::endl;
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
FecDecoderBase::set_max_packet_lifetime(const int64_t max_lifetime_ms) {
    m_max_packet_lifetime_ms = max_lifetime_ms;
    std::cerr << "max packet lifetime changed to " << max_lifetime_ms << std::endl;
}

void
FecDecoderBase::set_stats_window_size(const int32_t wnd_ms) {
    m_stat_window_ms = (std::min)(wnd_ms, (int32_t)kMaxStatWindowMs);
    std::cerr << "set stat window to " << m_stat_window_ms << std::endl;
}

void
FecDecoderBase::decode(const uint8_t* data, int len) {
    auto packet = FecPacket::parse_from_buffer(data, len);
    if (!packet) {
        return;
    }

    auto now_ms = Time::clocktime();
    auto header = packet->get_header();

    maybe_remove_outdated_decoders(now_ms, header->sequence_number);

    BandFecHeaderType bandfec_header;
    bandfec_parse_block((void*)packet->get_payload(), packet->get_payload_size(), bandfec_header);

    if (0 == m_seq_decoders.count(header->sequence_number)) {
        auto decoder = new Decoder;

        decoder->n              = bandfec_header.n;
        decoder->k              = bandfec_header.k;
        decoder->creation_time  = now_ms;
        decoder->decoder        = create_bandfec_decoder(&on_fec_receive, (int64_t)this, header->sequence_number);

        m_seq_decoders[header->sequence_number] = decoder;

        if (NumberUnwrapper<uint16_t>::is_newer_value(header->sequence_number, m_latest_sequence_num)) {
            m_latest_sequence_num = header->sequence_number;
            decoder->missing_groups = ((((uint32_t)header->sequence_number + 65536) - m_latest_sequence_num) % 65536);
        } else {
            std::cerr << "new sequence(" << header->sequence_number << ") is later than latest(" << m_latest_sequence_num << ")" << std::endl;
        }

        on_sequence_start(header->sequence_number, header, &bandfec_header);
    }

    decode_fec_block(header->sequence_number, &bandfec_header, packet->get_payload(), packet->get_payload_size(), header->red);
    packet->release();
}

void
FecDecoderBase::loss_stats(PacketLossStats& stats) {
    int64_t received_data_packets{ 0 }, expected_data_packets{ 0 }, recovered_packets{ 0 }, missing_groups{ 0 };
    int32_t loss_distribution[kMaxContLossCount + 1]{ 0 };

    {
        std::lock_guard<std::mutex> lock(m_mutex);

        for (auto& stat : m_recent_stats) {
            expected_data_packets += stat.second.expected_data_packets;
            received_data_packets += stat.second.received_data_packets;
            recovered_packets     += stat.second.recovered_packets;
            missing_groups        += stat.second.missing_groups;

            for (int i = 0; i < (int)(sizeof(loss_distribution) / sizeof(loss_distribution[0])); ++i) {
                loss_distribution[i] += stat.second.loss_distribution[i];
            }
        }
    }

    stats.lossrate           = (expected_data_packets - received_data_packets) * 1. / expected_data_packets;
    stats.effective_lossrate = (expected_data_packets - received_data_packets - recovered_packets) * 1. / expected_data_packets;
    stats.missing_groups     = missing_groups;

    memcpy(stats.loss_dist, loss_distribution, sizeof(loss_distribution));
}

int64_t
FecDecoderBase::get_max_packet_lifetime() const {
    return m_max_packet_lifetime_ms;
}

void
FecDecoderBase::maybe_remove_outdated_decoders(int64_t now_ms, int32_t reset_sequence_number) {
    std::vector<uint16_t> outdated_decoders;

    for (auto& decoder : m_seq_decoders) {
        if (now_ms - decoder.second->creation_time >= m_max_packet_lifetime_ms) {
            std::cerr << "removing overdue decoder: " << decoder.first << std::endl; 
            outdated_decoders.push_back(decoder.first);
        } else {
            if ((int32_t)decoder.first == reset_sequence_number) {
                decoder.second->max_consecutive_next_group_packets = 0;
            } else if (++decoder.second->max_consecutive_next_group_packets > m_max_forward_packets) {
                std::cerr << "removing decoder(seq: " << decoder.first << ") triggered by forward packets" << std::endl;
                outdated_decoders.push_back(decoder.first);
            }
        }
    }

    for (auto& decoder : outdated_decoders) {
        remove_decoder(decoder);
    }
}

void
FecDecoderBase::decode_fec_block(uint16_t sequence_number, const BandFecHeaderType* bandfec_header, const void* data, int len, bool red) {
    auto decoder = m_seq_decoders[sequence_number];

    if (red) {
        assert(bandfec_header->i >= decoder->n);
        assert(bandfec_header->i < decoder->n + decoder->k);
        assert(decoder->red_packets.size() < decoder->k);

        decoder->red_packets.push_back(bandfec_header->i);
    } else {
        assert(bandfec_header->i < decoder->n);
        assert(decoder->data_packets.size() < decoder->n);

        decoder->data_packets.push_back(bandfec_header->i);
    }
    /** sequential delivery is unnecessary(the decoder accepts out-of-order packets)
     */
    bandfec_decode(decoder->decoder, (void*)data, len);
}

void
FecDecoderBase::send_frame(uint16_t sequence_number, uint16_t frame_number, const uint8_t* data, int data_len) {
    m_observer->on_decoder_output(this, sequence_number, frame_number, data, data_len);
}

void
FecDecoderBase::collect_stats(uint16_t sequence) {
    auto decoder = m_seq_decoders[sequence];

    Stats stats;
    decoder->collect_stats(sequence, stats);

    auto now_ms = Time::clocktime();

    {
        std::lock_guard<std::mutex> lock(m_mutex);

        m_recent_stats[now_ms] = stats;

        while (!m_recent_stats.empty()) {
            auto first_stat = m_recent_stats.begin();
            if (now_ms - first_stat->first < m_stat_window_ms) {
                break;
            }

            m_recent_stats.erase(first_stat);
        }
    }

    m_global_stats.expected_data_packets += stats.expected_data_packets;
    m_global_stats.received_data_packets += stats.received_data_packets;
    m_global_stats.recovered_packets     += stats.recovered_packets;
}

void
FecDecoderBase::remove_decoder(uint16_t sequence) {
    if (0 == m_seq_decoders.count(sequence)) {
        return;
    }

    auto decoder = m_seq_decoders[sequence];

    flush_bandfec_decoder(decoder->decoder);
    destroy_bandfec_decoder(decoder->decoder);

    collect_stats(sequence);

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

void
FecDecoderBase::on_block_decoded(uint16_t sequence_number, int32_t pos, const uint8_t* data, int len) {
    auto decoder      = m_seq_decoders[sequence_number];
    auto itr          = std::find(decoder->data_packets.begin(), decoder->data_packets.end(), pos);
    auto is_recovered = itr == decoder->data_packets.end();

    assert(pos < decoder->n);
    if (is_recovered) {
        ++decoder->recovered_packets;
    }

    on_new_block(sequence_number, pos, data, len, is_recovered);
}
