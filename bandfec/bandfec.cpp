#include "bandfec.h"
#include "band_header.h"
#include "../utils/utils.h"

#include <algorithm>

static unsigned int poly[] = {
    0x1, 0x2, 0x7, 0xb, 0x13, 0x25, 0x43, 0x83, 0x11b, 0x203, 0x409, 0x805, 0x1009, 0x201b, 0x4021, 0x8003, 0x1002b
};

/** This function is used to send the redundant packets in a permutated order, distributing the effects of bust losses.
 *  The weakness of this implementation is that the last k % v packets are not permutated. We choose v = w.
 *  A technique without this drawback is: (v = w*2)
 *    kdw = f->e->k / f->e->w / 2;
 *    off2nd = i - (kdw + 1) * (f->e->k % (f->e->w * 2));
 *    i2redundant = off2nd < 0 ? i / (kdw + 1) + i % (kdw + 1) * f->e->w * 2 : f->e->k % (f->e->w * 2) + off2nd / kdw + off2nd % kdw * f->e->w * 2
 *
 */
static int
i2redundant(int i, int k, int v) {
    return i + k % v >= k ? i % v + k / v * v : i % v * (k / v) + i / v;
}

static void
MAC(int multiplier, int32_t* source, int32_t* dest, int g, int s) {
    /** multiply with scalar and then accumulate
     */
    int i, j, k, l;
    for (j = 0; j < s / (int)sizeof (*source); j += s / g / (int)sizeof (*source)) {
        for (k = multiplier, l = 0; k; k >>= 1, l += s / g / (int)sizeof (*source)) {
            if (k & 1) {
                for (i = 0; i < s / g / (int)sizeof (*source); ++i) {
                    dest[l + i] ^= source[j + i];
                }
            }
        }

        multiplier <<= 1;
        if (multiplier >= (1 << g)) {
            multiplier ^= poly[g];
        }
    }
}

BandFecEnc*
create_bandfec_encoder(uint16_t     s,
                       uint16_t     n,
                       uint16_t     k,
                       uint8_t      w,
                       uint8_t      g,
                       fec_send     cb_send,
                       int64_t      user_data1,
                       int64_t      user_data2) {
    if (s % (g * sizeof (int32_t)) != 0 || g > 16) {
        /** illegal Galois field size
         */
        return nullptr;
    }

    /** memory layout
     *
     *  .-----------------------.
     *  |     BandFecEncoder    |
     *  |-----------------------|
     *  |      redundant 0      |
     *  |-----------------------|
     *  |        ... ...        |
     *  |-----------------------|
     *  |    redundant k - 1    |
     *  |-----------------------|
     *  |     BandFecHeader     |
     *  |-----------------------|
     *  |           s           |
     *  .-----------------------.
     *
     */
    auto f = (BandFecEnc*)malloc(sizeof(BandFecEnc) + sizeof(BandFecHeader) + s * (k + 1));
    if (!f) {
        /** out of memory
         */
        free(f);
        return nullptr;
    }

    f->user_data1 = user_data1;
    f->user_data2 = user_data2;
    f->cb_send    = cb_send;

    f->e.s = s;
    f->e.n = n;
    f->e.k = k;
    f->e.w = w;
    f->e.g = g;
    f->e.i = 0;

    auto h = (BandFecHeader*)(s * k + (char*) (f + 1));

    h->s = UINT16_TO_BE(s);
    h->n = UINT16_TO_BE(n);
    h->k = UINT16_TO_BE(k);
    h->w = w;
    h->g = g;
    h->i = 0;

    memset(f + 1, 0, s * k); ///< initialize the redundant packets
    return f;
}

static void
add_to_redundant(int32_t* buf, BandFecEncDec* e, int i) {
    /** this is called by both the encoder and the decoder when they process the payload. But this code is also repeated where the decoder sets up the matrix. 
     *  i suppose a lot of pseudo random stuff can be tried, but in the end nature will add its own randomness by way of the packets it destroys.
     */
    int row, mid = i * e->w % e->k; ///< e.g. mid = i * 87654321 % e->k
    uint32_t coef = i + 1;

    for (row = (std::max)(0, (std::min)(mid, e->k - e->w) - e->w); row < (std::min)(e->k, (std::max)(mid, e->w) + e->w); ++row) {
        coef *= 1763689789;
        MAC(coef >> (32 - e->g), buf, (int32_t*)(row * e->s + (char*)(e + 1)), e->g, e->s);
    }

    /** what actually needs to be investigated is the problems at the edges:
     *  With "for (row = max (mid - e->w, 0); row < min (mid + e->w, e->k); row++)"
     *  there are columns with less than 2w non-zero entries which is a weakness.
     *  Apart for the current solution, another solution would be to have row "-1" wrap around into row k - 1 and
     *  row "k" into row 0 etc. The matrix will not be a band matrix which complicates things.
     *
     */
}

static void
send_data(int32_t* buf, BandFecEnc* f, bool red) {
    auto h = (BandFecHeader*)(f->e.k * f->e.s + (char*)(f + 1));
    int s = f->e.s + sizeof (*h);

    memcpy(h + 1, buf, f->e.s);

    h->i = UINT32_TO_BE(f->e.i);
    ++f->e.i;

    f->cb_send(f, h, s, red, f->user_data1, f->user_data2);
}

void
bandfec_encode(BandFecEnc* f, int32_t* buf, bool& done) {
    done = false;

    add_to_redundant(buf, &f->e, f->e.i);
    send_data(buf, f, false);

    if (f->e.i % (f->e.n + f->e.k) == f->e.n) {
        for (int i = 0; i < f->e.k; ++i) {
            send_data((int32_t*)(i2redundant(i, f->e.k, f->e.w) * f->e.s + (char*)(f + 1)), f, true);
        }

        done = true;
    }
}

void
destroy_bandfec_encoder(BandFecEnc* f) {
    free(f);
}

BandFecDec*
create_bandfec_decoder(fec_recv cb_recv, int64_t user_data1, int64_t user_data2) {
    BandFecDec* f = (BandFecDec*)malloc(sizeof(*f));

    f->user_data1           = user_data1;
    f->user_data2           = user_data2;
    f->cb_recv              = cb_recv;
    f->lost_packets         = 0;
    f->received_packets     = 0;
    f->corrected_packets    = 0;
    f->e                    = nullptr;
    f->nmissed              = 0;
    f->missed               = nullptr;

    return f;
}

void
bandfec_decode(BandFecDec* f, void* buf) {
    auto h = (BandFecHeader*)buf;
    int i, must_send = 0, hi = UINT32_FROM_BE(h->i);

    if (!f->e) {
        f->e             = (BandFecEncDec*)calloc(1, sizeof (*f->e) + UINT16_FROM_BE(h->s)*UINT16_FROM_BE(h->k));
        f->e->s          = UINT16_FROM_BE(h->s);
        f->e->n          = UINT16_FROM_BE(h->n);
        f->e->k          = UINT16_FROM_BE(h->k);
        f->e->w          = h->w;
        f->e->g          = h->g;
        f->e->i          = hi / (f->e->n + f->e->k) * (f->e->n + f->e->k);
        f->lost_packets += f->e->i;
    } else if (UINT16_FROM_BE(h->s) != f->e->s || UINT16_FROM_BE(h->n) != f->e->n || UINT16_FROM_BE(h->k) != f->e->k) {
        /** changing of FEC parameters not supported
         */
        return;
    }

    /** f->e->i is the one we are expecting
     */
    if (f->e->i <= hi) {
        /** if we got it, or a later one
         */
        must_send = 1;
        do {
            if (0 == (f->e->i % (f->e->n + f->e->k))) {
                flush_bandfec_decoder(f);
            }

            if (f->e->i < hi) {
                /** if we got a later one, this one is marked as awol
                 */
                f->missed               = (int32_t*)realloc(f->missed, (f->nmissed + 1) * sizeof (*f->missed));
                f->missed[f->nmissed++] = f->e->i;
            }
        } while (++f->e->i <= hi);
    } else {
        for (i = f->nmissed - 1; i >= 0; --i) {
            if (f->missed[i] == hi) {
                /** one of the awols showed up late
                 */
                f->nmissed--;
                f->missed[i] = f->missed[f->nmissed];
                must_send = 1;

                break; ///< don't bother to free() the 4 bytes.
            }
        }
    }

    if (must_send) {
        i = hi % (f->e->n + f->e->k) - f->e->n;
        if (i < 0) {
            add_to_redundant((int32_t*)(h + 1), f->e, hi);

            (*f->cb_recv)(f, (hi / (f->e->n + f->e->k) * f->e->n + i + f->e->n) * (int64_t) f->e->s, (int32_t*)(h + 1), f->e->s, f->user_data1, f->user_data2);
            ++f->received_packets;
        } else {
            MAC(1, (int32_t*)(h + 1), (int32_t*)(i2redundant (i, f->e->k, f->e->w) * f->e->s + (char*) (f->e + 1)), f->e->g, f->e->s);
        }
    }
}

void
flush_bandfec_decoder(BandFecDec* f) {
#define BITS ((int32_t)sizeof(int32_t) * 8) ///< the # of columns stored in each *coef
    struct Temp {
        int start, len, pivotLog, *coef;
        /** 'start' is in terms of missed packets / bits
         *  'len' is the number of words.
         *  'coef' has len groups of g "ints". 
         *  column "start" correspond to the least significant bits in the g "ints" of the first group.
         */
        int32_t* redundant;
    } *r, **matrix, *best;

    int i, tmp, row, mid, j, leader, bestLeader = 0, k, *GFlog, *GFexp;
    int32_t* final;
    uint32_t coef;

    while (f->e->i % (f->e->n + f->e->k) != 0) {
        f->missed               = (int32_t*)realloc(f->missed, (f->nmissed + 1) * sizeof (*f->missed));
        f->missed[f->nmissed++] = f->e->i++;
    }

    if (0 == f->nmissed) {
        return; ///< this happens at startup
    }

    if (f->nmissed > f->e->k) {
        for (i = 0; i < f->nmissed; ++i) {
            if (f->missed[i] % (f->e->n + f->e->k) < f->e->n) {
                ++f->lost_packets;
            }
        }

        f->nmissed = 0;
        free(f->missed);
        f->missed = nullptr;

        return;
    }
  
    r      = (Temp*)malloc(sizeof(*r) * f->e->k);
    matrix = (Temp**)malloc(sizeof(*matrix) * f->e->k);

    for (i = 0; i < f->e->k; ++i) {
        r[i].coef       = nullptr;
        r[i].start      = 0;
        r[i].len        = 0;
        r[i].pivotLog   = -1;
        r[i].redundant  = (int32_t*)(i * f->e->s + (char*) (f->e + 1));

        matrix[i] = r + i;
    }

    for (i = f->nmissed - 1; i >= 0; --i) {
        tmp = f->missed[i] % (f->e->n + f->e->k) - f->e->n;
        if (tmp >= 0) {
            /** drop the redundants we don't have
             */
            matrix[i2redundant(tmp, f->e->k, f->e->w)] = nullptr;
            f->missed[--f->nmissed]                    = f->missed[i];
        }
    }
  
    /** now f->missed only contains the payload packets
     */
    std::sort(f->missed, f->missed + f->nmissed,
        [ew = f->e->w, ek = f->e->k] (const int a, const int b) {
            return a * ew % ek < b * ew % ek;
        }
    );
  
    /** the sorting places all the nonzero entries in the matrix together
     */

    for (i = 0; i < f->nmissed; ++i) {
        /** build matrix
         */
        mid  = f->missed[i] * f->e->w % f->e->k; ///< e.g. mid = i * 87654321 % e->k
        coef = f->missed[i] + 1;

        for (row = (std::max)(0, (std::min)(mid, f->e->k - f->e->w) - f->e->w); row < (std::min)(f->e->k, (std::max)(mid, f->e->w) + f->e->w); ++row) {
            coef *= 1763689789;
            tmp  = coef >> (32 - f->e->g);

            if (0 == tmp || !matrix[row]) {
                continue;
            }

            if (r[row].start + r[row].len * BITS <= i) {
                /** need space?
                 */
                if (0 == r[row].len) {
                    r[row].start = i / BITS * BITS;
                }

                r[row].coef = (int*)realloc(r[row].coef, (i + BITS - r[row].start) / BITS * f->e->g * sizeof (int));

                memset(r[row].coef + r[row].len * f->e->g, 0, f->e->g * sizeof (int) * ((i - r[row].start) / BITS + 1 - r[row].len));
                r[row].len = (i - r[row].start) / BITS + 1;
            }

            for (j = (i - r[row].start) / BITS * f->e->g; tmp > 0; j++, tmp >>= 1) {
                if (tmp & 1) r[row].coef[j] ^= 1 << (i & (BITS - 1));
            } ///< shift the bits into the matrix
        } ///< for each row.
    } ///< for each column

    /** work out the Galois field
     */
    GFexp = (int*)malloc(sizeof(*GFexp) * (2i64 << f->e->g) - 2);
    GFlog = (int*)malloc(sizeof(*GFlog) * (1i64 << f->e->g));

    for (i = 0, tmp = 1; i < (2 << f->e->g) - 2; i++) {
        GFexp[i] = tmp;
        if (i < (1 << f->e->g) - 1) {
            GFlog[tmp] = i;
        }

        tmp <<= 1;
        if (tmp >> f->e->g) {
            tmp ^= poly[f->e->g];
        }
    }
  
    /** now the slow bit : creating the pivots rows
     *  Actually, if the system is overdetermined to a great degree
     *  (i.e. very few packets were lost) all the pivots may already exist, we just have to find them.
     */
    for (i = 0; i < f->nmissed;) {
        bestLeader = -1; ///< the leader is the first non zero element
        for (row = i; row < f->e->k && bestLeader < i; ++row) {
            if (!matrix[row]) {
                continue;
            }

            for (leader = 0; leader * BITS + matrix[row]->start <= i && leader < matrix[row]->len; ++leader) {
                for (j = k = 0; k < f->e->g; k++) {
                    j |= matrix[row]->coef[leader * f->e->g + k];
                }

                if (0 == j) {
                    continue; ///< row starts with 32 zeros
                }

                for (leader = leader * BITS + matrix[row]->start; !(j & 1); ++leader) {
                    j >>= 1;
                }

                if (leader > i || bestLeader >= i) {
                    break; ///< not a new best so break
                }

                bestLeader = leader;
                tmp        = row;

                break; ///< we worked out where the leader is
            }
        }

        if (bestLeader < 0) {
            break;
        }

        best        = matrix[tmp];
        matrix[tmp] = matrix[i];
        matrix[i]   = best;

        /** now eliminate* best from bestLeader to i
         */
        for (j = bestLeader; ; ++j) {
            leader = 0; ///< if j = i we calculate leader before quiting the loop.
            if ((j - best->start) / BITS < best->len) {
                for (k = ((j - best->start) / BITS + 1) * f->e->g - 1;
                k >= (j - best->start) / BITS * f->e->g; --k) {
                    leader <<= 1;
                    if (best->coef[k] & (1 << (j & (BITS - 1)))) {
                        ++leader;
                    }
                }
            }

            if (j >= i) {
                break; ///< bail out with "leader" the pivot
            }

            if (!leader) {
                continue; ///< multiplying with 0 has no effect
            }

            leader = GFexp[GFlog[leader] + (1<<f->e->g) - 1 - matrix[j]->pivotLog];
            /** now we want* best += best[j] / matrix[j]->pivot * matrix[j].
             *  redundants are easy:
             */
            MAC(leader, matrix[j]->redundant, best->redundant, f->e->g, f->e->s);
        
            /** the matrix is itself more tricky : have to check for space first.
             *  Note that with normal band matrices we can require that each row's tail not end before the row above it and then the code below would never excute.
             *  But this Gauss elimination is not normal, and there is always a possibility that we may end up needing a row that was put aside long ago.
             *
             */
            k = matrix[j]->start / BITS + matrix[j]->len - best->start / BITS - best->len;

            if (k > 0) {
                best->coef = (int*)realloc(best->coef, (best->len + k) * sizeof (best->coef[0]) * f->e->g);

                memset(best->coef + best->len * f->e->g, 0, k * f->e->g * sizeof (best->coef[0]));
                best->len += k;
            }
      
            for (k = (j - matrix[j]->start) / BITS, tmp = (j - best->start) / BITS; k < matrix[j]->len; k++, tmp++) {
                MAC(leader, matrix[j]->coef + k * f->e->g, best->coef + tmp * f->e->g, f->e->g, f->e->g * sizeof (int));
            }
        } ///< for each entry we eliminate

        if (leader != 0) {
            matrix[i++]->pivotLog = GFlog[leader];
        }
    } ///< for each pivot we need
  
    if (bestLeader < 0) {
        f->lost_packets += f->nmissed;
    } else {
        /** let's do back substitution
         */
        f->corrected_packets += f->nmissed;

        final = (int32_t*)malloc(f->e->s);
        for (i = f->nmissed - 1; i >= 0; --i) {
            memset(final, 0, f->e->s);

            MAC(GFexp[(1<<f->e->g) - 1 - matrix[i]->pivotLog],
            matrix[i]->redundant, final, f->e->g, f->e->s);

            /** now that we have final, we may as well back substitute into all
             */
            for (j = 0; j < i; ++j) {
                /** the rows above
                 */
                k = (i - matrix[j]->start) / BITS;
                if (k >= matrix[j]->len) {
                    continue;
                }

                for (leader = 0, tmp = f->e->g - 1; tmp >= 0; --tmp) {
                    leader = (leader << 1) + (1 & (matrix[j]->coef[k * f->e->g + tmp] >> (i & (BITS - 1))));
                } ///< we call MAC, even if leader is 0. Is it inefficient?

                MAC (leader, final, matrix[j]->redundant, f->e->g, f->e->s);
            }

            (*f->cb_recv)(f, (f->missed[i] / (f->e->n + f->e->k) * f->e->n + f->missed[i] % (f->e->n + f->e->k)) * (int64_t) f->e->s, final, f->e->s, f->user_data1, f->user_data2);
        }

        free (final);
    }

    free(GFlog);
    free(GFexp);
    free(matrix);
    free(f->missed);

    f->missed  = nullptr;
    f->nmissed = 0;

    for (i = 0; i < f->e->k; ++i) {
        free(r[i].coef);
    }
    free(r);
}

void
destroy_bandfec_decoder(BandFecDec* f) {
    if (f) {
        free(f->e);
        free(f);
    }
}
