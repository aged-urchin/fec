#include "fec_decoder_compact.h"
#include "./utils/number_unwrapper.h"
#include "./utils/utils.h"
#include "./utils/timeutils.h"

#include <iostream>

FecDecoderCompact::ReconstructedFrame::ReconstructedFrame(int size) {
    if (size > 0) {
        data.resize(size);
        creation_time = Time::clocktime();
    }
}

bool
FecDecoderCompact::ReconstructedFrame::ready() {
    int total_written = 0;
    for (const auto& slot : slots) {
        total_written += slot.second;
    }

    return !data.empty() && total_written == (int)data.size();
}

bool
FecDecoderCompact::ReconstructedFrame::push_fragment(int offset, const uint8_t* fragment_data, int size) {
    if (data.empty()) {
        std::cerr << "invalid operation" << std::endl;
        return false;
    }

    if (offset < 0 || size <= 0 || !fragment_data || offset + size > (int)data.size()) {
        std::cerr << "invalid arguments" << std::endl;
        return false;
    }

    /** determine if slot is available
     */
    int fragment_end = offset + size;
    for (const auto& slot : slots) {
        int slot_start = slot.first;
        int slot_end = slot_start + slot.second;

        if (offset < slot_end && fragment_end > slot_start) {
            /**!!! should never reach here, maybe forget to clean old pending frames or lifetime set too long?
             */
            std::cerr << "slot conflict" << std::endl;
            return false;
        }
    }

    std::copy(fragment_data, fragment_data + size, data.begin() + offset);
    slots[offset] = size;

    return true;
}

FecDecoderCompact::FecDecoderCompact(FecType type, IFecDecoderObserver* observer) :
FecDecoderBase(type, kFecModeCompact, observer) {

}

FecDecoderCompact::~FecDecoderCompact() {

}

void
FecDecoderCompact::destroy(PacketLossStats* stats) {
    destroy_decoders(stats);

    while (!m_pending_frames.empty()) {
        auto itr = m_pending_frames.begin();
        delete itr->second;

        m_pending_frames.erase(itr);
    }
}

void
FecDecoderCompact::clean_old_frames(uint16_t frame_number) {
    auto now_ms   = Time::clocktime();
    auto lifetime = get_max_packet_lifetime();
    auto deadline = now_ms - lifetime;

    for (auto itr = m_pending_frames.begin(); itr != m_pending_frames.end(); ) {
        if (deadline > itr->second->creation_time ||
            (NumberUnwrapper<uint16_t>::is_newer_value(frame_number, itr->first) && ((((uint32_t)frame_number + 65536) - itr->first) % 65536) > 300)) {

            std::cerr << "removing partially assembled frame: " << itr->first
                      << ", lifetime: " << now_ms - itr->second->creation_time << ", newest frame number is: " << frame_number
                      << ", total pending frames: " << m_pending_frames.size() << std::endl;

            delete itr->second;
            itr = m_pending_frames.erase(itr);
        } else {
            ++itr;
        }
    }
}

void
FecDecoderCompact::on_new_block(uint16_t sequence, int32_t, const uint8_t* data, int len, bool recovered) {
    auto remaining_data = (uint8_t*)data;
    auto remaining_len  = len;

    /** there may be some trailing trivial bytes(with size <= sizeof(FecFragmentHeader))
     */
    while (remaining_len > (int)sizeof(FecFragmentHeader)) {
        auto frag_header = (FecFragmentHeader*)remaining_data;
        auto header      = convert_fragment_to_host(frag_header);

        if (is_empty_fragment(&header)) {
            std::cerr << "empty fragment, skip this block" << std::endl;
            return;
        }

        remaining_data += sizeof(FecFragmentHeader);
        remaining_len  -= sizeof(FecFragmentHeader);

        if (remaining_len < header.frag_size) {
            std::cerr << "invalid fragment" << std::endl;
            return;
        }

        if (0 == m_pending_frames.count(header.frame_number)) {
            clean_old_frames(header.frame_number);
            m_pending_frames[header.frame_number] = new(std::nothrow) ReconstructedFrame(header.frame_size);
        }

        auto frame = m_pending_frames[header.frame_number];
        auto ret   = frame->push_fragment(header.frag_offset, remaining_data, header.frag_size);
        if (!ret) {
            std::cout << "pending frames: " << m_pending_frames.size() << ", oldest: " << m_pending_frames.begin()->first << ", newest: "
                      << m_pending_frames.rbegin()->first << ", current: " << header.frame_number << std::endl;
            return;
        }

        remaining_data += header.frag_size;
        remaining_len  -= header.frag_size;

        if (frame->ready()) {
            send_frame(sequence, header.frame_number, frame->data.data(), (int)frame->data.size());

            m_pending_frames.erase(header.frame_number);
            delete frame;
        }
    }
}
