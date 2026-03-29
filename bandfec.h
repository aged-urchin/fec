#ifndef ___BANDFEC_H___
#define ___BANDFEC_H___

#include <cstdint>

typedef void (*fec_send) (struct BandFecEncoder* f, void* buf, size_t size, bool red, int64_t user_data1, int64_t user_data2);
typedef void (*fec_recv) (struct BandFecDecoder* f, int64_t position, void* buf, int len, int64_t user_data, int64_t user_data2);

struct BandFecHeaderType {
    uint16_t    s; ///< blockSize, must be multiple of 4*g
    uint16_t    n;
    uint16_t    k;
    uint8_t     w;
    uint8_t     g;

    uint32_t    i;
};

struct BandFecEncDec {
    int32_t k, w, g, n, i, s;
};

struct BandFecEncoder {
    int64_t         user_data1;
    int64_t         user_data2;
    fec_send        cb_send;
    BandFecEncDec   e;
};

struct BandFecDecoder {
    int                 lost_packets;
    int                 received_packets;
    int                 corrected_packets;

    int64_t             user_data1;
    int64_t             user_data2;
    fec_recv            cb_recv;
    BandFecEncDec*      e;          ///< the redudant data is at e + 1
    int                 nmissed;
    int32_t*            missed;     ///< keeps track of both payload and redundant packets
};

BandFecEncoder*
create_bandfec_encoder(uint16_t     s,
                       uint16_t     n,
                       uint16_t     k,
                       uint8_t      w,
                       uint8_t      g,
                       fec_send     cb_send,
                       int64_t      user_data1,
                       int64_t      user_data2);

void
bandfec_encode(BandFecEncoder* f, int32_t* buf, bool& done);

void
destroy_bandfec_encoder(BandFecEncoder* f);

BandFecDecoder*
create_bandfec_decoder(fec_recv cb_recv, int64_t user_data1, int64_t user_data2);

void
bandfec_parse_block(void* buf, size_t size, BandFecHeaderType& header);

size_t
bandfec_decode(BandFecDecoder* f, void* buf, size_t size);

void
flush_bandfec_decoder(BandFecDecoder* f);

void
destroy_bandfec_decoder(BandFecDecoder* f);

#endif ///< ___BANDFEC_H___
