#include "fec_encoder_compact.h"
#include "fec_encoder_softrtp.h"
#include "fec_decoder_compact.h"
#include "fec_decoder_softrtp.h"

#include <cassert>

FecFragmentHeader kEndingFragHeader{ 0, 0, 0, 0 };

IFecEncoder*
create_fec_encoder(FecType type, FecMode mode, IFecEncoderObserver* observer) {
    assert(kFecTypeBand == type || kFecTypeRS == type || kFecTypeFastRS == type);
    assert(kFecModeCompact == mode || kFecModeSoftRtp == mode);

    if (kFecModeCompact == mode) {
        return new (std::nothrow) FecEncoderCompact(type, observer);
    } else if (kFecModeSoftRtp == mode) {
        return new (std::nothrow) FecEncoderSoftRtp(type, observer);
    }

    return nullptr;
}

void
destroy_fec_encoder(IFecEncoder* encoder) {
    delete encoder;
}

IFecDecoder*
create_fec_decoder(FecType type, FecMode mode, IFecDecoderObserver* observer) {
    if (kFecModeCompact == mode) {
        return new (std::nothrow) FecDecoderCompact(type, observer);
    } else if (kFecModeSoftRtp == mode) {
        return new (std::nothrow) FecDecoderSoftRtp(type, observer);
    }

    assert(0);
    return nullptr;
}

void
destroy_fec_decoder(IFecDecoder* decoder, PacketLossStats* stats) {
    if (!decoder) {
        return;
    }

    auto mode = decoder->mode();
    if (kFecModeCompact == mode) {
        ((FecDecoderCompact*)decoder)->destroy(stats);
    } else if (kFecModeSoftRtp == mode) {
        ((FecDecoderSoftRtp*)decoder)->destroy(stats);
    }

    delete decoder;
}
