#ifndef ___UTILS_H___
#define ___UTILS_H___

#include "../fec_codec.h"

#include <new>
#include <cassert>
#include <cstring>

#define UINT16_TO_BE(val)       (uint16_t)((((val) >> 8) & 0xFF) | (((val) & 0x00FF) << 8))
#define UINT32_TO_BE(val)       (uint32_t)((((val) >> 24) & 0xFF) | (((val) >>  8) & 0x0000FF00) | (((val) <<  8) & 0x00FF0000) | (((val) << 24) & 0xFF000000))
#define UINT16_FROM_BE(val)     UINT16_TO_BE(val)
#define UINT32_FROM_BE(val)     UINT32_TO_BE(val)

extern FecFragmentHeader kEndingFragHeader;

struct RtpHeader {
    unsigned char      cc : 4;
    unsigned char      x : 1;
    unsigned char      p : 1;
    unsigned char      v : 2;

    unsigned char      pt : 7;
    unsigned char      m : 1;

    uint16_t           seq;
    uint32_t           ts;

    uint32_t           ssrc;
};

static bool
parse_rtp_buffer(const uint8_t* buffer, uint16_t size, RtpHeader& hdr) {
    assert(buffer != nullptr);
    assert(size > sizeof(RtpHeader));
    if (!buffer || size <= sizeof(RtpHeader)) {
        return false;
    }

    memcpy(&hdr, buffer, sizeof(RtpHeader));
    if (hdr.v != 2) {
        return false;
    }

    hdr.seq  = UINT16_FROM_BE(hdr.seq);
    hdr.ts   = UINT32_FROM_BE(hdr.ts);
    hdr.ssrc = UINT32_FROM_BE(hdr.ssrc);

    return true;
}

static bool
is_empty_fragment(const FecFragmentHeader* header) {
    return 0 == header->frame_number && 0 == header->frame_size && 0 == header->frag_offset && 0 == header->frag_size;
}

static FecFragmentHeader
convert_fragment_to_network(const FecFragmentHeader* header) {
    FecFragmentHeader network_header;

    network_header.frame_number = UINT16_TO_BE(header->frame_number);
    network_header.frame_size   = UINT16_TO_BE(header->frame_size);
    network_header.frag_offset  = UINT16_TO_BE(header->frag_offset);
    network_header.frag_size    = UINT16_TO_BE(header->frag_size);

    return network_header;
}

static FecFragmentHeader
convert_fragment_to_host(const FecFragmentHeader* header) {
    FecFragmentHeader host_header;

    host_header.frame_number    = UINT16_FROM_BE(header->frame_number);
    host_header.frame_size      = UINT16_FROM_BE(header->frame_size);
    host_header.frag_offset     = UINT16_FROM_BE(header->frag_offset);
    host_header.frag_size       = UINT16_FROM_BE(header->frag_size);

    return host_header;
}

static FecHeader*
create_fec_header(int ext_size = 0) {
    auto header_size = sizeof(FecHeader) + ext_size;
    auto ptr         = new(std::nothrow) uint8_t[header_size];

    memset(ptr, 0, header_size);
    return (FecHeader*)ptr;
}

static void
destroy_fec_header(FecHeader* header) {
    delete[] (char*)header;
}

static int32_t
fec_mode_to_value(FecMode mode) {
    if (kFecModeCompact == mode) {
        return 0;
    } else if (kFecModeSoftRtp == mode) {
        return 1;
    }

    return -1;
}

static FecMode
fec_mode_from_value(int32_t value) {
    if (0 == value) {
        return kFecModeCompact;
    } else if (1 == value) {
        return kFecModeSoftRtp;
    }

    return kFecModeNull;
}

static int32_t
fec_type_to_value(FecType type) {
    if (kFecTypeBand == type) {
        return 0;
    } else if (kFecTypeRS == type) {
        return 1;
    }

    return -1;
}

static FecType
fec_type_from_value(int32_t value) {
    if (0 == value) {
        return kFecTypeBand;
    } else if (1 == value) {
        return kFecTypeRS;
    }

    return kFecTypeNull;
}
#endif ///< ___UTILS_H___
