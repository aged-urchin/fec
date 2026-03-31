#include "fec_encoder.h"
#include "fec_decoder.h"
#include "fec_encoder2.h"
#include "fec_decoder2.h"

FecFragmentHeader kEndingFragHeader{ 0, 0, 0, 0 };

const int
fec_header_size(const uint8_t type) {
    if (kFecExtNull == type) {
        return sizeof(FecHeader);
    } else if (kFecExtRtp == type) {
        return sizeof(FecHeader) + sizeof(RtpFecExt);
    }

    return 0;
}

FecHeader*
create_empty_fec_header(const uint8_t type) {
    if (kFecExtNull == type) {
        return new FecHeader();
    } else if (kFecExtRtp == type) {
        auto ptr = new char[fec_header_size(type)];
        return (FecHeader*)ptr;
    }

    return nullptr;
}

void
destroy_fec_header(FecHeader* header) {
    if (kFecExtNull == header->typ) {
        delete header;
    } else if (kFecExtRtp == header->typ) {
        delete[] (char*)header;
    }
}

FecHeader*
duplicate_fec_header(const FecHeader* header) {
    auto header2 = create_empty_fec_header(header->typ);
    memcpy(header2, header, fec_header_size(header->typ));

    return header2;
}

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
destroy_fec_decoder2(IFecDecoder* decoder) {
    delete decoder;
}