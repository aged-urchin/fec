#ifndef ___BAND_HEADER_H___
#define ___BAND_HEADER_H___

#include <cstdint>

#pragma pack(push, 1)
struct BandFecHeader {
    uint16_t    s; ///< blockSize, must be multiple of 4*g
    uint16_t    n;
    uint16_t    k;
    uint8_t     w;
    uint8_t     g;

    uint32_t    i; ///< 0,...
};
#pragma pack(pop)

void
bandfec_parse_block(void* buf, BandFecHeader& header);

#endif ///< ___BAND_HEADER_H___
