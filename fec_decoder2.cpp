#include "fec_decoder2.h"
#include <iostream>

FecDecoder2::FecDecoder2(IFecDecoderObserver* observer) :
FecDecoderBase(observer) {

}

FecDecoder2::~FecDecoder2() {
    destroy_decoders();
}

void
FecDecoder2::on_new_block(uint16_t sequence_number, int32_t pos, const uint8_t* data, int len) {
    RtpHeader rtp_header;
    if (!parse_rtp_buffer(data, len, rtp_header)) {
        std::cerr << "decoder outputs an invalid rtp packet" << std::endl;
        return;
    }

    /** use 'rtp sequence number' as the 2nd param?
     */
    send_frame(sequence_number, rtp_header.seq, data, len);
}
