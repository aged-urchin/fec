#include "band_header.h"
#include "../utils/utils.h"

void
bandfec_parse_block(void* buf, BandFecHeader& header) {
    auto h = (BandFecHeader*)buf;

    header.s = UINT16_FROM_BE(h->s);
    header.n = UINT16_FROM_BE(h->n);
    header.k = UINT16_FROM_BE(h->k);
    header.w = h->w;
    header.g = h->g;
    header.i = UINT32_FROM_BE(h->i);
}

