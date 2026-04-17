#ifndef ___TEST3_H___
#define ___TEST3_H___

#include "network_conditioner.h"
#include <set>

namespace TEST3 {

/** add redundancy via packet retransmission
 */
void test() {
    RandomLossTool traffic(LossRateType::LOSS_30_PERCENT);

    std::vector<int32_t> packets;
    std::set<int32_t> recovered_packets;

    const int32_t kRepeat = 1;
    const int32_t kTotalDataPackets = 10000;

    for (int32_t i = 0; i < kTotalDataPackets; ++i) {
        packets.push_back(i);

        for (int32_t j = 0; j < kRepeat; ++j) {
            packets.push_back(i);
        }
    }

    int32_t num_received_packets = 0;
    for (auto& i : packets) {
        if (!traffic.is_packet_lost()) {
            ++num_received_packets;
            recovered_packets.insert(i);
        }
    }

    int32_t lost_packets = 0;
    int32_t prev = 0;

    for (auto it = recovered_packets.begin(); it != recovered_packets.end(); ++it) {
        auto curr = *it;
        auto gap = curr - prev - 1;

        if (gap > 0) {
            lost_packets += gap;
        }

        prev = curr;
    }

    printf("lossrate: %f%%, effective loss: %f%%\n",
            (packets.size() - num_received_packets) * 100.0 / packets.size(), lost_packets * 100.0 / kTotalDataPackets);
}

}
#endif ///< ___TEST3_H___
