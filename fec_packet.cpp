#include "fec_packet.h"
#include <iostream>

FecPacket*
FecPacket::create_instance(const uint8_t* data, int len, uint16_t sequence_number) {
    auto packet = new FecPacket(data, len, sequence_number);
    return packet;
}

FecPacket*
FecPacket::parse_from_buffer(const void* data, int len) {
    if (len <= sizeof(m_header)) {
        std::cout << "invalid argument" << std::endl;
        return nullptr;
    }

    auto header = (FecHeader*)data;
    auto packet = new FecPacket((uint8_t*)data + sizeof(m_header), len - sizeof(m_header), header->sequence_number);

    return packet;
}

FecPacket::FecPacket(const uint8_t* data, int len, uint16_t sequence_number) {
    m_header.sequence_number = sequence_number;

    m_data.insert(m_data.end(), (char*)&m_header, (char*)&m_header + sizeof(m_header));
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
