#include "fec_encoder.h"
#include "fec_encoder2.h"
#include "fec_decoder.h"
#include "fec_decoder2.h"
#include "fec_decoder_proxy.h"

#include <cassert>

FecFragmentHeader kEndingFragHeader{ 0, 0, 0, 0 };

IFecEncoder*
create_fec_encoder(FecType type, FecMode mode, IFecEncoderObserver* observer) {
    assert(kFecTypeBand == type || kFecTypeRS == type || kFecTypeFastRS == type);
    assert(kFecModeCompact == mode || kFecModeSoftRtp == mode);

    if (kFecModeCompact == mode) {
        return new (std::nothrow) FecEncoder(type, observer);
    } else if (kFecModeSoftRtp == mode) {
        return new (std::nothrow) FecEncoder2(type, observer);
    }

    return nullptr;
}

void
destroy_fec_encoder(IFecEncoder* encoder) {
    delete encoder;
}

IFecDecoder*
create_fec_decoder(IFecDecoderObserver* observer) {
    auto decoder = new(std::nothrow) FecDecoderProxy(observer);
    return decoder;
}

void
destroy_fec_decoder(IFecDecoder* decoder, PacketLossStats* stats) {
    auto fec_decoder = (FecDecoderProxy*)decoder;
    if (fec_decoder) {
        fec_decoder->destroy(stats);
    }

    delete fec_decoder;
}

IFecDecoder*
create_fec_decoder2(FecType type, FecMode mode, IFecDecoderObserver* observer) {
    if (kFecModeCompact == mode) {
        return new (std::nothrow) FecDecoder(type, observer);
    } else if (kFecModeSoftRtp == mode) {
        return new (std::nothrow) FecDecoder2(type, observer);
    }

    assert(0);
    return nullptr;
}

void
destroy_fec_decoder2(IFecDecoder* decoder, PacketLossStats* stats) {
    if (!decoder) {
        return;
    }

    auto mode = decoder->mode();
    if (kFecModeCompact == mode) {
        ((FecDecoder*)decoder)->destroy(stats);
    } else if (kFecModeSoftRtp == mode) {
        ((FecDecoder2*)decoder)->destroy(stats);
    }

    delete decoder;
}
