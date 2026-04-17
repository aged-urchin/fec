#include "fec_decoder_proxy.h"
#include "fec_decoder.h"
#include "fec_decoder2.h"
#include "fec_packet.h"
#include "./utils/utils.h"

#include <algorithm>
#include <iostream>

FecDecoderProxy::FecDecoderProxy(IFecDecoderObserver* observer) :
m_observer(observer) {

}

FecDecoderProxy::~FecDecoderProxy() {
    destroy_real_decoder();
}

void
FecDecoderProxy::destroy(PacketLossStats* stats) {
    if (!m_decoder) {
        return;
    }

    auto mode = m_decoder->mode();
    if (kFecModeCompact == mode) {
        ((FecDecoder*)m_decoder)->destroy(stats);
    } else if (kFecModeSoftRtp == mode) {
        ((FecDecoder2*)m_decoder)->destroy(stats);
    }
}

void
FecDecoderProxy::set_max_forward_packets(int packets) {
    m_cached_max_forward_packets     = packets;
    m_cached_max_forward_packets_set = true;

    if (m_decoder) {
        m_decoder->set_max_forward_packets(packets);
    }
}

void
FecDecoderProxy::set_max_packet_lifetime(const int64_t max_lifetime_ms) {
    m_cached_max_lifetime_ms  = max_lifetime_ms;
    m_cached_max_lifetime_set = true;

    if (m_decoder) {
        m_decoder->set_max_packet_lifetime(max_lifetime_ms);
    }
}

void
FecDecoderProxy::set_stats_window_size(const int32_t wnd_ms) {
    m_cached_stats_wnd_ms  = wnd_ms;
    m_cached_stats_wnd_set = true;

    if (m_decoder) {
        m_decoder->set_stats_window_size(wnd_ms);
    }
}

void
FecDecoderProxy::decode(const uint8_t* data, int len) {
    auto packet = FecPacket::parse_from_buffer(data, len);
    if (!packet) {
        std::cerr << "buffering one block before creating decoder" << std::endl;
        m_buffered_blocks.push_back({ data, data + len });
        return;
    }

    auto header = packet->get_header();

    auto new_type = fec_type_from_value(header->typ);
    if (kFecTypeNull == new_type) {
        std::cerr << "invalid fec type " << (int)header->typ << std::endl;
        return;
    }

    auto new_mode = fec_mode_from_value(header->mod);
    if (kFecModeNull == new_mode) {
        std::cerr << "invalid fec mode " << (int)header->mod << std::endl;
        return;
    }

    if (!m_decoder ||
        new_type != m_current_type || new_mode != m_decoder->mode()) {
        rebuild_decoder(new_type, new_mode);
    }

    if (m_decoder) {
        if (!m_buffered_blocks.empty()) {
            std::for_each(m_buffered_blocks.begin(), m_buffered_blocks.end(), [this](std::vector<uint8_t>& block) {
                m_decoder->decode(block.data(), (int)block.size());
            });
            m_buffered_blocks.clear();
        }
        m_decoder->decode(data, len);
    }
}

void
FecDecoderProxy::loss_stats(PacketLossStats& stats) {
    if (m_decoder) {
        m_decoder->loss_stats(stats);
    }
}

FecMode
FecDecoderProxy::mode() {
    if (m_decoder) {
        return m_decoder->mode();
    }

    return kFecModeNull;
}

void
FecDecoderProxy::destroy_real_decoder() {
    if (m_decoder) {
        std::cerr << "destroy decoder, type: " << (int)m_current_type << ", mode: " << (int)m_decoder->mode() << std::endl;
        destroy(nullptr);

        delete m_decoder;
        m_decoder = nullptr;
    }
}

void
FecDecoderProxy::rebuild_decoder(FecType new_type, FecMode new_mode) {
    destroy_real_decoder();

    switch (new_mode) {
    case kFecModeCompact:
        m_decoder = new FecDecoder(new_type, m_observer);
        break;
    case kFecModeSoftRtp:
        m_decoder = new FecDecoder2(new_type, m_observer);
        break;
    default:
        return;
    }

    if (!m_decoder) {
        return;
    }

    if (m_cached_max_forward_packets_set) {
        m_decoder->set_max_forward_packets(m_cached_max_forward_packets);
    }
    if (m_cached_max_lifetime_set) {
        m_decoder->set_max_packet_lifetime(m_cached_max_lifetime_ms);
    }
    if (m_cached_stats_wnd_set) {
        m_decoder->set_stats_window_size(m_cached_stats_wnd_ms);
    }

    m_current_type = new_type;
    std::cerr << "new decoder created, type: " << (int)new_type << ", mode: " << (int)new_mode << std::endl;
}
