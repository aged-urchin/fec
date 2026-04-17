#include "fec_packet.h"
#include "./utils/utils.h"

#include <cstddef>
#include <cassert>

FecPacket*
FecPacket::create_instance(const FecHeader* header, const uint8_t* data, int len) {
    return new(std::nothrow) FecPacket(header, data, len);
}

FecPacket*
FecPacket::parse_from_buffer(const void* data, int len) {
    if (!is_fec_packet(data, len)) {
        return nullptr;
    }

    return new(std::nothrow) FecPacket((const uint8_t*)data, len);
}

bool
FecPacket::is_fec_packet(const void* data, int len) {
    auto buf = (const uint8_t*)data;
    if (!data || len <= (int)sizeof(FecHeader)) {
        return false;
    }

    uint8_t sig = (buf[0] >> 6) & 0x03; ///< 0..1
    uint8_t ver = (buf[0] >> 4) & 0x03; ///< 2..3
    uint8_t typ = (buf[0] >> 2) & 0x03; ///< 4..5
    uint8_t mod = buf[0] & 0x03;        ///< 6..7

    uint8_t red           = (buf[1] >> 7) & 0x01; ///< 8
    uint8_t sid           = (buf[1] >> 4) & 0x07; ///< 9..11

    if (sig != 3) {
        /** not fec
         */
        return false;
    }

    if (ver != 0) {
        /** we only support version 0 for now
         */
        return false;
    }

    if ((typ != 0 && typ != 1) || (mod != 0 && mod != 1)) {
        /** invalid arguments
         */
        return false;
    }

    if (sid != 0) {
        /** not supported
         */
        return false;
    }

    /** mod: 0 is kFecModeCompact, 1 is kFecModeSoftRtp
     */
    if (1 == mod && !red) {
        /** encoder only outputs red fec packets under 'kFecModeSoftRtp'
         */
        return false;
    }

    return true;
}

FecPacket::FecPacket(const FecHeader* header, const uint8_t* data, int len) {
    m_header = create_fec_header(ext_size(header, header->mod));
    std::memcpy(m_header, header, header_size(header));

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
FecPacket::get_packet_buffer() const {
    return m_data.data();
}

uint32_t
FecPacket::get_packet_buffer_size() const {
    return (uint32_t)m_data.size();
}

const void*
FecPacket::get_payload() const {
    return m_data.data() + header_size(m_header);
}

uint32_t
FecPacket::get_payload_size() const {
    return (uint32_t)m_data.size() - (uint32_t)header_size(m_header);
}

std::vector<uint8_t>
FecPacket::header_2_network(const FecHeader* header) {
    std::vector<uint8_t> buffer;
    uint8_t byte0 = 0, byte1 = 0;

    byte0 |= (header->sig & 0x03) << 6;
    byte0 |= (header->ver & 0x03) << 4;
    byte0 |= (header->typ & 0x03) << 2;
    byte0 |= (header->mod & 0x03);

    byte1 |= (header->red & 0x01) << 7;
    byte1 |= (header->sid & 0x07) << 4;

    buffer.push_back(byte0);
    buffer.push_back(byte1);

    buffer.push_back((uint8_t)(header->sequence_number >> 8) & 0xff);
    buffer.push_back((uint8_t)(header->sequence_number & 0xff));

    if (kFecModeSoftRtp == fec_mode_from_value(header->mod)) {
        auto rtp_ext = (SoftRtp*)header->ext;

        buffer.push_back((uint8_t)(rtp_ext->base_sequence_num >> 8) & 0xff);
        buffer.push_back((uint8_t)(rtp_ext->base_sequence_num & 0xff));

        buffer.push_back(rtp_ext->delta_size_bytes);
        assert(rtp_ext->delta_size_bytes > 0);

        const uint8_t* src_ptr         = rtp_ext->delta_size;
        int            remaining_bytes = rtp_ext->delta_size_bytes;

        while (remaining_bytes-- > 0) {
            buffer.push_back(*src_ptr++);
        }
    }

    return buffer;
}

FecHeader*
FecPacket::header_from_network(const void* data, int len) {
    auto bytes = (uint8_t*)data;

    auto sig = (bytes[0] >> 6) & 0x03;
    auto ver = (bytes[0] >> 4) & 0x03;
    auto typ = (bytes[0] >> 2) & 0x03;
    auto mod = (bytes[0] >> 0) & 0x03;

    auto red = (bytes[1] >> 7) & 0x01;
    auto sid = (bytes[1] >> 4) & 0x07;

    auto host_header = create_fec_header(ext_size(data, mod));

    host_header->sig = sig;
    host_header->ver = ver;
    host_header->typ = typ;
    host_header->mod = mod;
    host_header->red = red;
    host_header->sid = sid;

    host_header->sequence_number = UINT16_FROM_BE(*(uint16_t*)&bytes[2]);

    if (kFecModeSoftRtp == fec_mode_from_value(host_header->mod)) {
        auto rtp_ext = (SoftRtp*)host_header->ext;

        rtp_ext->base_sequence_num = UINT16_FROM_BE(*(uint16_t*)&bytes[4]);
        rtp_ext->delta_size_bytes  = bytes[6];

        const uint8_t* src_ptr = &bytes[7];
        uint8_t*       dst_ptr = rtp_ext->delta_size;

        std::memcpy(dst_ptr, src_ptr, rtp_ext->delta_size_bytes);
    }

    return host_header;
}

int
FecPacket::header_size(const FecHeader* header) const {
    if (!header) {
        return 0;
    }

    return sizeof(FecHeader) + ext_size(header, header->mod);
}

int
FecPacket::ext_size(const void* data, int mod) const {
    if (fec_mode_from_value(mod) == kFecModeSoftRtp) {
        auto rtp_ext = (SoftRtp*)((uint8_t*)data + offsetof(FecHeader, ext));
        assert(rtp_ext->delta_size_bytes >= 1);

        return sizeof(SoftRtp) + rtp_ext->delta_size_bytes - 1;
    }

    return 0;
}
