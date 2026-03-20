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
    m_header.sig = 3; ///< '11'
    m_header.typ = 0;
    m_header.sid = 0;
    m_header.red = is_red ? 1 : 0;

    m_header.reserved        = 0;
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
    std::vector<uint8_t> buffer;
    uint8_t byte0 = 0;

    byte0 |= (header.sig & 0x03) << 6;
    byte0 |= (header.typ & 0x03) << 4;
    byte0 |= (header.sid & 0x07) << 1;
    byte0 |= (header.red & 0x01) << 0;

    buffer.push_back(byte0);
    buffer.push_back(header.reserved);

    uint16_t seq_net = UINT16_TO_BE(header.sequence_number);
    buffer.push_back((uint8_t)(seq_net >> 8) & 0xff);
    buffer.push_back((uint8_t)(seq_net & 0xff));

    return buffer;
}

FecHeader
FecPacket::header_from_network(const void* data, int len) {
    FecHeader host_header;

    auto bytes = (uint8_t*)data;

    host_header.sig = (bytes[0] >> 6) & 0x03;
    host_header.typ = (bytes[0] >> 4) & 0x03;
    host_header.sid = (bytes[0] >> 1) & 0x07;
    host_header.red = (bytes[0] >> 0) & 0x01;

    host_header.reserved = bytes[1];
    host_header.sequence_number = UINT16_FROM_BE(*(uint16_t*)&bytes[2]);

    return host_header;
}
