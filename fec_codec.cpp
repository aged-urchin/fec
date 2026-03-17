#include "bandfec_encoder.h"
#include "bandfec_decoder.h"

IFecEncoder*
create_fec_encoder(IFecEncoderObserver* observer) {
    auto encoder = new BandFecEncoder(observer);
    return encoder;
}

void
destroy_fec_encoder(IFecEncoder* encoder) {
    delete encoder;
}

IFecDecoder*
create_fec_decoder(IFecDecoderObserver* observer) {
    auto decoder = new BandFecDecoder(observer);
    return decoder;
}

void
destroy_fec_decoder(IFecDecoder* decoder) {
    delete decoder;
}
