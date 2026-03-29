#ifndef ___FEC_PACKET_H___
#define ___FEC_PACKET_H___

#include "fec_codec.h"
#include "refcount.h"

#include <vector>

class FecPacket : public IFecPacket,
                  public RefCount {
public:
    static FecPacket* create_instance(const uint8_t* data, int len, uint16_t sequence_number, bool is_red);

    static FecPacket* parse_from_buffer(const void* data, int len);

    static bool is_fec_packet(const void* data, int len);

    unsigned long retain() override {
        return RefCount::retain();
    }

    unsigned long release() override {
        return RefCount::release();
    }

    bool get_header(FecHeader& header) const override;

    const void* get_buffer() const override;

    uint32_t get_buffer_size() const override;

    const void* get_payload() const override;

    uint32_t get_payload_size() const override;

private:
    FecPacket(const uint8_t* data, int len, uint16_t sequence_number, bool is_red);

    ~FecPacket() override;

    std::vector<uint8_t> header_2_network(const FecHeader* header);

    static FecHeader header_from_network(const void* data, int len);

private:

    FecHeader*              m_header{ nullptr };
    std::vector<uint8_t>    m_data;
};

#endif ///< ___FEC_PACKET_H___
