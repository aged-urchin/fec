#include "fec_decoder_base.h"
#include "fec_packet.h"
#include "bandfec/bandfec_decoder.h"
#include "cm256/cm256_decoder.h"
#include "./utils/number_unwrapper.h"
#include "./utils/utils.h"
#include "./utils/timeutils.h"

#include <iostream>
#include <sstream>
#include <cassert>
#include <set>

void
FecDecoderBase::Decoder::collect_stats(uint16_t sequence, Stats& stats, FecType type) {
    auto num_data_packets      = (int32_t)data_packets.size();
    auto num_red_packets       = (int32_t)red_packets.size();
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
        auto gap = curr - prev - 1;

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
        if (kFecTypeBand == type) {
            std::cerr << "maybe RS code could recover this group(" << sequence << ")!" << std::endl;
        } else {
            /** The Reed-Solomon algorithm should be capable of recovering up to n−k lost packets
             */
            assert(0);
        }
    }

    std::cerr << "finish sequence: " << sequence << "(n: " << n << ", k: " << k << ")"
              << ", " << data_str.str() << ", " << red_str.str()
              << ", recovered: " << num_recovered_packets << ", lost: " << n - num_data_packets - num_recovered_packets
              << ", intact: " << std::boolalpha << intact << std::endl;
}

FecDecoderBase::FecDecoderBase(FecType type, FecMode mode, IFecDecoderObserver* observer) :
m_type(type),
m_mode(mode),
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
    assert(fec_mode_from_value(header->mod) == m_mode);

    maybe_remove_outdated_decoders(now_ms, header->sequence_number);

    auto info = get_header_info((void*)packet->get_payload(), packet->get_payload_size());

    if (0 == m_seq_decoders.count(header->sequence_number)) {
        auto decoder = new(std::nothrow) Decoder;
        assert(decoder);

        decoder->n              = info.n;
        decoder->k              = info.k;
        decoder->creation_time  = now_ms;
        decoder->decoder        = create_decoder(header->sequence_number);

        m_seq_decoders[header->sequence_number] = decoder;

        if (NumberUnwrapper<uint16_t>::is_newer_value(header->sequence_number, m_latest_sequence_num)) {
            m_latest_sequence_num = header->sequence_number;
            decoder->missing_groups = ((((uint32_t)header->sequence_number + 65536) - m_latest_sequence_num) % 65536);
        } else {
            std::cerr << "new sequence(" << header->sequence_number << ") is later than latest(" << m_latest_sequence_num << ")" << std::endl;
        }

        on_sequence_start(header->sequence_number, header, &info);
    }

    decode_fec_block(header->sequence_number, &info, packet->get_payload(), packet->get_payload_size(), header->red);
    packet->release();
}

void
FecDecoderBase::loss_stats(PacketLossStats& stats) {
    Stats sum_stat;
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        for (auto& stat : m_recent_stats) {
            sum_stat.expected_data_packets += stat.second.expected_data_packets;
            sum_stat.received_data_packets += stat.second.received_data_packets;
            sum_stat.recovered_packets     += stat.second.recovered_packets;
            sum_stat.missing_groups        += stat.second.missing_groups;

            for (int i = 0; i < (int)(sizeof(sum_stat.loss_distribution) / sizeof(sum_stat.loss_distribution[0])); ++i) {
                sum_stat.loss_distribution[i] += stat.second.loss_distribution[i];
            }
        }
    }

    convert_stats(&stats, sum_stat);
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
FecDecoderBase::decode_fec_block(uint16_t sequence_number, const FecHeaderInfo* info, const void* data, int len, bool red) {
    auto decoder = m_seq_decoders[sequence_number];

    if (red) {
        assert((int32_t)info->i >= decoder->n);
        assert((int32_t)info->i < decoder->n + decoder->k);
        assert((int32_t)decoder->red_packets.size() < decoder->k);

        decoder->red_packets.push_back(info->i);
    } else {
        assert((int32_t)info->i < decoder->n);
        assert((int32_t)decoder->data_packets.size() < decoder->n);

        decoder->data_packets.push_back(info->i);
    }
    /** sequential delivery is unnecessary(the decoder accepts out-of-order packets)
     */
    assert(decoder->decoder);
    decoder->decoder->decode((void*)data);
}

void
FecDecoderBase::send_frame(uint16_t sequence_number, uint16_t frame_number, const uint8_t* data, int data_len) {
    m_observer->on_decoder_output(this, sequence_number, frame_number, data, data_len);
}

FecHeaderInfo
FecDecoderBase::get_header_info(const void* block, int32_t size) {
    FecHeaderInfo info;

    if (kFecTypeBand == m_type) {
        BandFecDecoder::parse(block, size, info);
    } else if (kFecTypeRS == m_type) {
        CM256Decoder::parse(block, size, info);
    } else {
        assert(0);
        std::cerr << "invalid type " << (int)m_type << std::endl;
    }

    return info;
}

IFecDecoderAdapter*
FecDecoderBase::create_decoder(uint16_t sequence) {
    IFecDecoderAdapter* decoder = nullptr;

    if (kFecTypeBand == m_type) {
        decoder = new(std::nothrow) BandFecDecoder();
    } else if (kFecTypeRS == m_type) {
        decoder = new(std::nothrow) CM256Decoder();
    }

    if (!decoder || !decoder->create(sequence, this)) {
        std::cerr << "could not create decoder adapter" << std::endl;

        delete decoder;
        return nullptr;
    }
    
    return decoder;
}

void
FecDecoderBase::collect_stats(uint16_t sequence) {
    auto decoder = m_seq_decoders[sequence];

    Stats stats;
    decoder->collect_stats(sequence, stats, m_type);

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

    for (int i = 0; i < (int)(sizeof(stats.loss_distribution) / sizeof(stats.loss_distribution[0])); ++i) {
        m_global_stats.loss_distribution[i] += stats.loss_distribution[i];
    }

    m_global_stats.expected_data_packets += stats.expected_data_packets;
    m_global_stats.received_data_packets += stats.received_data_packets;
    m_global_stats.recovered_packets     += stats.recovered_packets;
    m_global_stats.missing_groups        += stats.missing_groups;
}

void
FecDecoderBase::remove_decoder(uint16_t sequence) {
    if (0 == m_seq_decoders.count(sequence)) {
        return;
    }

    auto decoder = m_seq_decoders[sequence];
    assert(decoder->decoder);

    decoder->decoder->flush();
    delete decoder->decoder;
    decoder->decoder = nullptr;

    collect_stats(sequence);

    delete decoder;
    m_seq_decoders.erase(sequence);

    on_sequence_end(sequence);
}

void
FecDecoderBase::destroy_decoders(PacketLossStats* stats) {
    while (!m_seq_decoders.empty()) {
        auto itr = m_seq_decoders.begin();
        remove_decoder(itr->first);
    }

    if (stats) {
        convert_stats(stats, m_global_stats);
    }
}

void
FecDecoderBase::on_decoder_output(IFecDecoderAdapter* adapter, uint16_t sequence_number, int32_t pos, const uint8_t* data, int32_t len) {
    auto decoder      = m_seq_decoders[sequence_number];
    auto itr          = std::find(decoder->data_packets.begin(), decoder->data_packets.end(), pos);
    auto is_recovered = itr == decoder->data_packets.end();

    assert(pos < decoder->n);
    if (is_recovered) {
        ++decoder->recovered_packets;
    }

    on_new_block(sequence_number, pos, data, len, is_recovered);
}

void
FecDecoderBase::convert_stats(PacketLossStats* packet_stats, const Stats& stats) {
    assert(packet_stats);

    packet_stats->lossrate           = (stats.expected_data_packets - stats.received_data_packets) * 1.f / stats.expected_data_packets;
    packet_stats->effective_lossrate = (stats.expected_data_packets - stats.received_data_packets - stats.recovered_packets) * 1.f / stats.expected_data_packets;
    packet_stats->missing_groups     = stats.missing_groups;

    assert(sizeof(stats.loss_distribution[0]) == sizeof(packet_stats->loss_dist[0]));
    assert(sizeof(stats.loss_distribution) == sizeof(packet_stats->loss_dist));

    memcpy(packet_stats->loss_dist, stats.loss_distribution, sizeof(stats.loss_distribution));
}
