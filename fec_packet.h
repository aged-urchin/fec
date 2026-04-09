#ifndef ___FEC_PACKET_H___
#define ___FEC_PACKET_H___

#include "fec_codec.h"
#include "refcount.h"

#include <vector>

class FecPacket : public IFecPacket,
                  public RefCount {
public:
    static FecPacket* create_instance(const FecHeader* header, const uint8_t* data, int len);

    static FecPacket* parse_from_buffer(const void* data, int len);

    static bool is_fec_packet(const void* data, int len);

    unsigned long retain() override {
        return RefCount::retain();
    }

    unsigned long release() override {
        return RefCount::release();
    }

    const FecHeader* get_header() const override;

    const void* get_packet_buffer() const override;

    uint32_t get_packet_buffer_size() const override;

    const void* get_payload() const override;

    uint32_t get_payload_size() const override;

private:
    FecPacket(const FecHeader* header, const uint8_t* data, int len);

    FecPacket(const uint8_t* data, int len);

    ~FecPacket() override;

    int header_size(const FecHeader* header) const;

    int ext_size(const FecHeader* header) const;

    std::vector<uint8_t> header_2_network(const FecHeader* header);

    FecHeader* header_from_network(const void* data, int len);

private:

    FecHeader*              m_header{ nullptr };
    std::vector<uint8_t>    m_data;
};

#endif ///< ___FEC_PACKET_H___
