#ifndef ___BANDFEC_H___
#define ___BANDFEC_H___

#include <cstdint>

typedef void (*fec_send) (struct FecEncoder* f, void* buf, size_t size, int64_t user_data1, int64_t user_data2);
typedef void (*fec_recv) (struct FecDecoder* f, int64_t position, void* buf, int len, int64_t user_data, int64_t user_data2);

struct HeaderType {
    uint16_t    s; ///< blockSize, must be multiple of 4*g
    uint16_t    n;
    uint16_t    k;
    uint8_t     w;
    uint8_t     g;

    uint32_t    i;
};

struct FecEncDec {
    int32_t k, w, g, n, i, s;
};

struct FecEncoder {
    int64_t     user_data1;
    int64_t     user_data2;
    fec_send    cb_send;
    FecEncDec   e;
};

struct FecDecoder {
    int             lost_packets;
    int             received_packets;
    int             corrected_packets;

    int64_t         user_data1;
    int64_t         user_data2;
    fec_recv        cb_recv;
    FecEncDec*      e;          ///< the redudant data is at e + 1
    int             nmissed;
    int32_t*        missed;     ///< keeps track of both payload and redundant packets
};

FecEncoder*
create_fec_encoder(uint16_t     s,
                   uint16_t     n,
                   uint16_t     k,
                   uint8_t      w,
                   uint8_t      g,
                   fec_send     cb_send,
                   int64_t      user_data1,
                   int64_t      user_data2);

void
fec_encode(FecEncoder* f, int32_t* buf, bool& done);

void
destroy_fec_encoder(FecEncoder* f);

FecDecoder*
create_fec_decoder(fec_recv cb_recv, int64_t user_data1, int64_t user_data2);

void
fec_parse_block(void* buf, size_t size, HeaderType& header);

size_t
fec_decode(FecDecoder* f, void* buf, size_t size);

void
flush_fec_decoder(FecDecoder* f);

void
destroy_fec_decoder(FecDecoder* f);

#endif ///< ___BANDFEC_H___
