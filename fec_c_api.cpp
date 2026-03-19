#include "fec_c_api.h"
#include "fec_codec.h"

struct FecEncoderWrapper : IFecEncoderObserver {
    IFecEncoder*           encoder;
    fec_cb_encoder_output  cb;

    void on_encoder_output(IFecEncoder* encoder, IFecPacket* packet) override {
        cb(this, packet->get_buffer(), packet->get_buffer_size());
    }
};

struct FecDecoderWrapper : IFecDecoderObserver {
    IFecDecoder*           decoder;
    fec_cb_decoder_output  cb;

    void on_decoder_output(IFecDecoder*     decoder,
                           uint16_t         sequence_number,
                           uint16_t         frame_number,
                           const uint8_t*   data,
                           int              data_len) {
        cb(this, data, data_len);
    }
};

FEC_SHARED_API
void*
fec_encoder_create(fec_cb_encoder_output observer) {
    auto wrapper = new FecEncoderWrapper;

    wrapper->cb      = observer;
    wrapper->encoder = create_fec_encoder(wrapper);

    return wrapper;
}

FEC_SHARED_API
int
fec_encoder_set_param(void* encoder, int block_size_in_bytes, int data_blocks_in_group, int redundant_blocks_in_group) {
    auto wrapper = (FecEncoderWrapper*)encoder;
    auto ret     = wrapper->encoder->set_param(block_size_in_bytes, data_blocks_in_group, redundant_blocks_in_group);

    return ret ? 0 : -1;
}

FEC_SHARED_API
int
fec_encoder_encode(void* encoder, const void* data, int data_len) {
    auto wrapper = (FecEncoderWrapper*)encoder;
    wrapper->encoder->encode((const uint8_t*)data, data_len);

    return 0;
}

FEC_SHARED_API
int
fec_encoder_flush(void* encoder) {
    auto wrapper = (FecEncoderWrapper*)encoder;
    wrapper->encoder->flush();

    return 0;
}

FEC_SHARED_API
void
fec_encoder_destroy(void* encoder) {
    auto wrapper = (FecEncoderWrapper*)encoder;
    destroy_fec_encoder(wrapper->encoder);
    delete wrapper;
}

FEC_SHARED_API
void*
fec_decoder_create(fec_cb_decoder_output observer) {
    auto wrapper = new FecDecoderWrapper();

    wrapper->cb      = observer;
    wrapper->decoder = create_fec_decoder(wrapper);

    return wrapper;
}

FEC_SHARED_API
int
fec_decoder_decode(void* decoder, const void* data, int data_len) {
    auto wrapper = new FecDecoderWrapper();

    wrapper->decoder->decode((const uint8_t*)data, data_len);
    return 0;
}

FEC_SHARED_API
void
fec_decoder_destroy(void* decoder) {
    auto wrapper = new FecDecoderWrapper();

    fec_decoder_destroy(wrapper->decoder);
    delete wrapper;
}
