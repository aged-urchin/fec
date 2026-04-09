#include "fec_encoder.h"
#include "fec_decoder.h"
#include "fec_encoder2.h"
#include "fec_decoder2.h"

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
destroy_fec_decoder(IFecDecoder* decoder, PacketLossStats& stats) {
    auto fec_decoder = (FecDecoder*) decoder;
    if (fec_decoder) {
        fec_decoder->destroy();
        fec_decoder->loss_stats(stats);
    }

    delete fec_decoder;
}

IFecEncoder*
create_fec_encoder2(IFecEncoderObserver* observer) {
    auto encoder = new FecEncoder2(observer);
    return encoder;
}

void
destroy_fec_encoder2(IFecEncoder* encoder) {
    delete encoder;
}

IFecDecoder*
create_fec_decoder2(IFecDecoderObserver* observer) {
    auto decoder = new FecDecoder2(observer);
    return decoder;
}

void
destroy_fec_decoder2(IFecDecoder* decoder, PacketLossStats& stats) {
    auto fec_decoder = (FecDecoder2*)decoder;
    if (fec_decoder) {
        fec_decoder->destroy();
        fec_decoder->loss_stats(stats);
    }

    delete fec_decoder;
}
