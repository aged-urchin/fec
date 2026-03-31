#ifndef ___UTILS_H___
#define ___UTILS_H___

#include <cstdint>
#include <cassert>
#include <cstring>

#define UINT16_TO_BE(val)       (uint16_t)((((val) >> 8) & 0xFF) | (((val) & 0x00FF) << 8))
#define UINT32_TO_BE(val)       (uint32_t)((((val) >> 24) & 0xFF) | (((val) >>  8) & 0x0000FF00) | (((val) <<  8) & 0x00FF0000) | (((val) << 24) & 0xFF000000))
#define UINT16_FROM_BE(val)     UINT16_TO_BE(val)
#define UINT32_FROM_BE(val)     UINT32_TO_BE(val)

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

#endif ///< ___UTILS_H___
