#include "bandfec_decoder.h"
#include "bandfec.h"

void
on_fec_receive(FecDecoder* f, int64_t position, void* buf, int len, int64_t user_data1, int64_t user_data2) {
    auto decoder  = (BandFecDecoder*)user_data1;
    auto sequence = (uint32_t)user_data2;

    if (position % len) {
        printf("error\n");
    }

    decoder->on_new_block(sequence, (int32_t)position / len, (const uint8_t*)buf, len);
}

BandFecDecoder::BlockDecoder::BlockDecoder(const Config& _config) :
config(_config) {

}

BandFecDecoder::BlockDecoder::~BlockDecoder() {
    flush_fec_decoder(decoder);
    destroy_fec_decoder(decoder);
}

bool
BandFecDecoder::BlockDecoder::is_same(const Config& _config) const {
    return config.block_size == _config.block_size && config.blocks == _config.blocks &&
           config.red_blocks == _config.red_blocks && config.param_g == _config.param_g && config.param_w == _config.param_w;
}

BandFecDecoder::BandFecDecoder(IBandFecDecoderObserver* observer) :
m_observer(observer) {

}

BandFecDecoder::~BandFecDecoder() {
    while (!m_decoders.empty()) {
        delete_decoder(m_decoders.begin()->first);
    }
}

void
BandFecDecoder::max_reorder_delay(int delay_in_packets) {

}

void
BandFecDecoder::decode(uint32_t sequence, const uint8_t* data, int len) {
    BlockDecoder::Config config;
    BlockDecoder* decoder = nullptr;

    HeaderType header;
    if (len <= sizeof(HeaderType)) {
        printf("error\n");
        return;
    }

    fec_parse_block((uint8_t*)data, len, header);

    config.block_size = header.s;
    config.blocks     = header.n;
    config.red_blocks = header.k;
    config.param_w    = header.w;
    config.param_g    = header.g;

    int index = header.i;

    if (m_decoders.count(sequence) > 0 && !(decoder = m_decoders[sequence])->is_same(config)) {
        /** should not happen
         */
        printf("error\n");

        delete_decoder(sequence);
        return;
    }

    if (!decoder) {
        if (kMaxDecoders == m_decoders.size()) {
            delete_decoder(m_decoders.begin()->first);
        }

        decoder = new BlockDecoder(config);
        decoder->decoder = create_fec_decoder(&on_fec_receive, (int64_t)this, sequence);

        m_decoders[sequence] = decoder;
    }

    /** decoders that have no input data are dying
     */
    std::vector<uint32_t> outdated_decoders;
    for (auto& dec : m_decoders) {
        if (decoder != dec.second) {
            ++dec.second->no_packets_cnt;
            if (dec.second->no_packets_cnt >= dec.second->kDeathCounterOnNoData) {
                printf("decoder(%u) is dead\n", dec.first);
                outdated_decoders.push_back(dec.first);
            }
        } else {
            decoder->no_packets_cnt = 0;
        }
    }
    /** purge dead decoders
     */
    for (auto& dec : outdated_decoders) {
        delete_decoder(dec);
    }

    auto blocks = decoder->reorder.add_block(index, data, len);
    for (auto& block : blocks) {
        fec_decode(decoder->decoder, block.second.data(), block.second.size());
    }
}

void
BandFecDecoder::delete_decoder(uint32_t sequence) {
    if (0 == m_decoders.count(sequence)) {
        return;
    }

    auto decoder = m_decoders[sequence];

    auto blocks = decoder->reorder.flush();
    for (auto& block : blocks) {
        fec_decode(decoder->decoder, block.second.data(), block.second.size());
    }

    delete decoder;
    m_decoders.erase(sequence);
}

void
BandFecDecoder::on_new_block(uint32_t sequence, int32_t pos, const uint8_t* data, int len) {
    m_observer->on_decoder_output(this, sequence, pos, data, len);
}
