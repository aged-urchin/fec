#ifndef ___FEC_C_API_H___
#define ___FEC_C_API_H___

#include <cstdint>

#if defined(FEC_SHARED_EXPORTS)
#define FEC_SHARED_API __attribute__((visibility("default")))
#else
#define FEC_SHARED_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*fec_cb_encoder_output)(void* encoder, const void* packet_data, int packet_len);
typedef void (*fec_cb_decoder_output)(void* decoder, const void* frame_data, int frame_len);

FEC_SHARED_API
void*
fec_encoder_create(fec_cb_encoder_output observer);

FEC_SHARED_API
int
fec_encoder_set_param(void* encoder, int block_size_in_bytes, int data_blocks_in_group, int redundant_blocks_in_group);

FEC_SHARED_API
int
fec_encoder_encode(void* encoder, const void* data, int data_len);

FEC_SHARED_API
int
fec_encoder_flush(void* encoder);

FEC_SHARED_API
void
fec_encoder_destroy(void* encoder);

FEC_SHARED_API
void*
fec_decoder_create(fec_cb_decoder_output observer);

FEC_SHARED_API
int
fec_decoder_decode(void* decoder, const void* data, int data_len);

FEC_SHARED_API
void
fec_decoder_destroy(void* decoder);

#ifdef __cplusplus
}
#endif

#endif ///< ___FEC_C_API_H___
