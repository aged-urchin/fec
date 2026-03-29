#include "fec_encoder.h"
#include "fec_decoder.h"

FecFragmentHeader kEndingFragHeader{ 0, 0, 0, 0 };

IFecEncoder*
create_fec_encoder(IFecEncoderObserver* observer) {
    auto encoder = new FecEncoder(observer);
    return encoder;
}

void
destroy_fec_encoder(IFecEncoder* encoder) {
    delete encoder;
}

IFecDecoder*
create_fec_decoder(IFecDecoderObserver* observer) {
    auto decoder = new FecDecoder(observer);
    return decoder;
}

void
destroy_fec_decoder(IFecDecoder* decoder) {
    delete decoder;
}
