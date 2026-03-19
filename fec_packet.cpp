#include "fec_packet.h"
#include <iostream>

FecPacket*
FecPacket::create_instance(const uint8_t* data, int len, uint16_t sequence_number, bool is_red) {
    auto packet = new FecPacket(data, len, sequence_number, is_red);
    return packet;
}

FecPacket*
FecPacket::parse_from_buffer(const void* data, int len) {
    if (!is_fec_packet(data, len)) {
        std::cout << "invalid argument" << std::endl;
        return nullptr;
    }

    auto header = header_from_network(data, len);
    auto packet = new FecPacket((uint8_t*)data + sizeof(m_header), len - sizeof(m_header), header.sequence_number, header.red);

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

    if (starting_bits != 3) {
        return false;
    }

    return true;
}

FecPacket::FecPacket(const uint8_t* data, int len, uint16_t sequence_number, bool is_red) {
    m_header.starting_bits   = 3; ///< '11'
    m_header.red             = is_red ? 1 : 0;
    m_header.reserved_1      = 0;
    m_header.version         = 0;
    m_header.reserved_2      = 0;
    m_header.sequence_number = sequence_number;

    auto be_header = header_2_network(m_header);

    m_data.insert(m_data.end(), be_header.begin(), be_header.end());
    m_data.insert(m_data.end(), data, data + len);
}

bool
FecPacket::get_header(FecHeader& header) const {
    header = m_header;
    return true;
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
    return m_data.data() + sizeof(m_header);
}

uint32_t
FecPacket::get_payload_size() const {
    return m_data.size() - sizeof(m_header);
}

std::vector<uint8_t>
FecPacket::header_2_network(const FecHeader& header) {
    uint16_t seq = UINT16_TO_BE(header.sequence_number);

    uint8_t send_buf[4] = { 0 };
    send_buf[0] = ((header.starting_bits & 0x03) << 6) | ((header.red & 0x01) << 5) | ((header.reserved_1 & 0x01) << 4) | (header.version & 0x0F);
    send_buf[1] = header.reserved_2 & 0xFF;

    memcpy(&send_buf[2], &seq, 2);
    return { send_buf, send_buf + 4 };
}

FecHeader
FecPacket::header_from_network(const void* data, int len) {
    FecHeader header = { 0 };
    auto buf = (const uint8_t*)data;

    header.starting_bits = (buf[0] >> 6) & 0x03;
    header.red           = (buf[0] >> 5) & 0x01;
    header.reserved_1    = (buf[0] >> 4) & 0x01;
    header.version       = buf[0] & 0x0F;
    header.reserved_2    = buf[1];

    uint16_t seq_net;
    memcpy(&seq_net, &buf[2], 2);
    header.sequence_number = UINT16_FROM_BE(seq_net);

    return header;
}
