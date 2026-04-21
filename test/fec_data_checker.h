#ifndef ___FEC_DATA_CHECKER_H___
#define ___FEC_DATA_CHECKER_H___

#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <ctime>

class FecChecker {
public:
    FecChecker() {
        srand((unsigned)time(nullptr));
    }

    bool check(const void* packet, unsigned int bytes) const {
        const uint8_t* buffer = (const uint8_t*)packet;
        if (!packet || bytes < 8) {
            return false;
        }

        if (bytes < 16) {
            if (bytes < 2) {
                return false;
            }
            uint8_t v = buffer[0];
            for (unsigned i = 1; i < bytes; ++i) {
                if (buffer[i] != v) {
                    return false;
                }
            }
            return true;
        }

        uint32_t read_len = read_u32(buffer + bytes - 8);
        uint32_t read_crc = read_u32(buffer + bytes - 4);

        if (read_len != bytes) {
            return false;
        }

        uint32_t calc_crc = crc32(buffer, bytes - 8);
        return read_crc == calc_crc;
    }

    bool generate(uint8_t* out_packet, unsigned max_size, unsigned& out_len) {
        if (!out_packet || max_size < 8) {
            return false;
        }

        out_len = 8 + rand() % (max_size - 7);
        if (out_len < 16) {
            uint8_t v = (uint8_t)rand();
            memset(out_packet, v, out_len);
        } else {
            for (unsigned i = 0; i < out_len - 8; ++i) {
                out_packet[i] = (uint8_t)rand();
            }

            write_u32(out_packet + out_len - 8, out_len);

            uint32_t calc_crc = crc32(out_packet, out_len - 8);
            write_u32(out_packet + out_len - 4, calc_crc);
        }
        return true;
    }

    unsigned int add_check(const uint8_t* src, unsigned int src_len, uint8_t* dst, unsigned int dst_max) const {
        if (!src || !dst || dst_max < src_len + 8) {
            return 0;
        }

        memcpy(dst, src, src_len);

        unsigned int total_len = src_len + 8;

        write_u32(dst + src_len, total_len);

        uint32_t calc_crc = crc32(src, src_len);
        write_u32(dst + src_len + 4, calc_crc);

        return total_len;
    }

private:
    uint32_t read_u32(const uint8_t* p) const {
        uint32_t v;
        memcpy(&v, p, 4);
        return v;
    }

    void write_u32(uint8_t* p, uint32_t v) const {
        memcpy(p, &v, 4);
    }

    uint32_t crc32(const uint8_t* data, uint32_t len) const {
        uint32_t crc = 0xFFFFFFFF;
        for (uint32_t i = 0; i < len; i++) {
            crc ^= data[i];
            for (int j = 0; j < 8; j++) {
                crc = (crc >> 1) ^ (0xEDB88320 * (crc & 1));
            }
        }
        return crc ^ 0xFFFFFFFF;
    }
};

#endif ///< ___FEC_DATA_CHECKER_H___
