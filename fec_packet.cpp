#include "fec_packet.h"
#include <iostream>

FecPacket*
FecPacket::create_instance(const FecHeader* header, const uint8_t* data, int len) {
    auto packet = new FecPacket(header, data, len);
    return packet;
}

FecPacket*
FecPacket::parse_from_buffer(const void* data, int len) {
    if (!is_fec_packet(data, len)) {
        std::cout << "invalid argument" << std::endl;
        return nullptr;
    }

    auto packet = new FecPacket((const uint8_t*)data, len);
    return packet;
}

bool
FecPacket::is_fec_packet(const void* data, int len) {
    auto buf = (const uint8_t*)data;
    if (!data || len <= sizeof(FecHeader)) {
        return false;
    }

    uint8_t first_byte    = buf[0];
    uint8_t starting_bits = (first_byte >> 6) & 0x03;
    uint8_t type          = (first_byte >> 4) & 0x03;
    uint8_t red           = (first_byte >> 0) & 0x01;

    if (starting_bits != 3) {
        return false;
    }

    if (type != 0 && type != 1) {
        /** kFecExtNull/kFecExtRtp
         */
        return false;
    }
/*
    if (1 == type && !red) {
        return false;
    }
*/
    return true;
}

FecPacket::FecPacket(const FecHeader* header, const uint8_t* data, int len) {
    m_header = duplicate_fec_header(header);

    auto be_header = header_2_network(m_header);

    m_data.insert(m_data.end(), be_header.begin(), be_header.end());
    m_data.insert(m_data.end(), data, data + len);
}

FecPacket::FecPacket(const uint8_t* data, int len) {
    m_header = header_from_network(data, len);
    m_data.insert(m_data.end(), data, data + len);
}

FecPacket::~FecPacket() {
    destroy_fec_header(m_header);
    m_header = nullptr;
}

const FecHeader*
FecPacket::get_header() const {
    return m_header;
}

const void*
FecPacket::get_buffer() const {
    return m_data.data();
}

uint32_t
FecPacket::get_buffer_size() const {
    return m_data.size();
}

const void*
FecPacket::get_payload() const {
    return m_data.data() + fec_header_size(m_header->typ);
}

uint32_t
FecPacket::get_payload_size() const {
    return m_data.size() - fec_header_size(m_header->typ);
}

std::vector<uint8_t>
FecPacket::header_2_network(const FecHeader* header) {
    std::vector<uint8_t> buffer;
    uint8_t byte0 = 0;

    byte0 |= (header->sig & 0x03) << 6;
    byte0 |= (header->typ & 0x03) << 4;
    byte0 |= (header->sid & 0x07) << 1;
    byte0 |= (header->red & 0x01) << 0;

    buffer.push_back(byte0);
    buffer.push_back(header->reserved);

    uint16_t seq_net = UINT16_TO_BE(header->sequence_number);
    buffer.push_back((uint8_t)(seq_net & 0xff));
    buffer.push_back((uint8_t)(seq_net >> 8) & 0xff);

    if (kFecExtRtp == header->typ) {
        auto rtp_ext = (RtpFecExt*)header->ext;

        uint16_t base_sequence_num = UINT16_TO_BE(rtp_ext->base_sequence_num);
        buffer.push_back((uint8_t)(base_sequence_num & 0xff));
        buffer.push_back((uint8_t)(base_sequence_num >> 8) & 0xff);

        uint16_t num_packets       = UINT16_TO_BE(rtp_ext->num_packets);
        buffer.push_back((uint8_t)(num_packets & 0xff));
        buffer.push_back((uint8_t)(num_packets >> 8) & 0xff);
    }

    return buffer;
}

FecHeader*
FecPacket::header_from_network(const void* data, int len) {
    auto bytes       = (uint8_t*)data;
    auto type        = (bytes[0] >> 4) & 0x03;

    auto host_header = create_empty_fec_header(type);

    host_header->sig = (bytes[0] >> 6) & 0x03;
    host_header->typ = type;
    host_header->sid = (bytes[0] >> 1) & 0x07;
    host_header->red = (bytes[0] >> 0) & 0x01;

    host_header->reserved        = bytes[1];
    host_header->sequence_number = UINT16_FROM_BE(*(uint16_t*)&bytes[2]);

    if (kFecExtRtp == host_header->typ) {
        auto rtp_ext = (RtpFecExt*)host_header->ext;

        rtp_ext->base_sequence_num = UINT16_FROM_BE(*(uint16_t*)&bytes[4]);
        rtp_ext->num_packets       = UINT16_FROM_BE(*(uint16_t*)&bytes[6]);
    }

    return host_header;
}
