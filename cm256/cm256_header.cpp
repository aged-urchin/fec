#include "cm256_header.h"
#include "cm256.h"
#include "../utils/utils.h"

#include <mutex>

void
cm256_parse_block(const void* buf, CM256Header& header) {
    auto h = (CM256Header*)buf;

    header.s = UINT16_FROM_BE(h->s);
    header.n = h->n;
    header.k = h->k;
    header.i = h->i;
}

bool
init_cm256() {
    static bool       cm256_inited = false;
    static std::mutex cm256_init_mutex;

    {
        std::lock_guard<std::mutex> lock(cm256_init_mutex);

        if (!cm256_inited) {
            if (cm256_init()) {
                return false;
            }

            cm256_inited = true;
        }
    }
    return true;
}
