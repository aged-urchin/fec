#include "leo_header.h"
#include "leopard.h"
#include "../utils/utils.h"

#include <mutex>

void
leopard_parse_block(const void* buf, LeopardHeader& header) {
    auto h = (LeopardHeader*)buf;

    header.s = UINT16_FROM_BE(h->s);
    header.n = UINT16_FROM_BE(h->n);
    header.k = UINT16_FROM_BE(h->k);
    header.i = UINT16_FROM_BE(h->i);
}

bool
init_leopard() {
    static bool       leopard_inited = false;
    static std::mutex leopard_init_mutex;

    {
        std::lock_guard<std::mutex> lock(leopard_init_mutex);

        if (!leopard_inited) {
            if (leo_init()) {
                return false;
            }

            leopard_inited = true;
        }
    }
    return true;
}
