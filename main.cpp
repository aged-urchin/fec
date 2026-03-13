#include "network_conditioner.h"
#include "bandfec_encoder.h"
#include "bandfec_decoder.h"
#include "reorder.h"

#include <iostream>
#include <iomanip>
#include <vector>
#include <string>
#include <fstream>
#include <windows.h>
#include <cassert>


#define USE_RANDOM_FILE false
RandomLossTool traffic(LossRateType::LOSS_30_PERCENT);

const int kFecParamS = 1024;
const int kFecParamN = 10;
const int kFecParamK = 10;

FILE* random_in = nullptr, *recv_block = nullptr;
std::vector<int32_t> random_numbers_in;

int num_pass = 0, num_rejects = 0, out_packets = 0, out_size = 0, recv_packets = 0, packet_lost = 0;

class Foo : public IBandFecEncoderObserver,
            public IBandFecDecoderObserver {
public:
    void start(const char* name) {
        m_out_file = fopen(name, "wb");

        random_in  = fopen("random_in.txt", "w");
        recv_block = fopen("recv_block.txt", "w");
#if USE_RANDOM_FILE
        std::ifstream file("read_random.txt");
        std::string line;

        while (std::getline(file, line)) {
            random_numbers_in.push_back(std::stoi(line));
        }
#endif

        m_encoder = new BandFecEncoder(this);
        m_decoder = new BandFecDecoder(this);

        m_encoder->set_param(kFecParamS, kFecParamN, kFecParamK);
    }

    void stop() {
        delete m_encoder;
        m_encoder = nullptr;

        delete m_decoder;
        m_decoder = nullptr;

        assert(m_reorders.empty());
    }

    void push_data(const std::vector<uint8_t>& data) {
        m_encoder->encode(data.data(), data.size());
    }

    void update(int N, int K) {
        m_encoder->set_param(kFecParamS, N, K);
    }

private:
    void on_encoder_output(BandFecEncoder* encoder, uint32_t sequence, const uint8_t* data, int len) override {
        auto block_idx = get_block_index(data, len);

        ++out_packets;
        out_size += len;

#if USE_RANDOM_FILE
        if (random_numbers_in.end() != std::find(random_numbers_in.begin(), random_numbers_in.end(), out_packets)) {
#else
		if (!traffic.is_packet_lost()) {
#endif
            fprintf(random_in, "%d\n", out_packets);

            ++num_pass;
            std::cout << u8"\u2714" << " [" << std::setw(7) << block_idx << "]: ";

            m_decoder->decode(sequence, data, len);
        } else {
            ++num_rejects;
            std::cout << u8"\u2718" << " [" << std::setw(7) << block_idx << "]: ";
        }

        if (!m_new_line) {
            std::cout << std::endl;
        }
        m_new_line = false;
    }

    void on_decoder_sequence_begin(BandFecDecoder* decoder, uint32_t sequence) override {
        fprintf(recv_block, "sequence %d ++++++\n", sequence);

        auto sequence_data = new SequenceData;
        sequence_data->reorder = new Reorder(20);

        m_reorders[sequence] = sequence_data;
        if (-1 == m_active_sequence) {
            m_active_sequence = sequence;
        }
    }

    void on_decoder_sequence_end(BandFecDecoder* decoder, uint32_t sequence) override {
        fprintf(recv_block, "sequence %d ------\n", sequence);
        if (m_reorders.count(sequence) > 0) {
            auto sequence_data = m_reorders[sequence];

            assert(sequence_data->buffered_data.empty());

            delete sequence_data->reorder;
            delete sequence_data;

            m_reorders.erase(sequence);

            if (m_reorders.empty()) {
                m_active_sequence = -1;
            } else {
                m_active_sequence = m_reorders.begin()->first;
            }
        } else {
            std::cout << "error no such a sequence " << sequence << std::endl;
            assert(0);
        }
    }

    void on_decoder_output(BandFecDecoder* decoder, uint32_t sequence, int32_t pos, const uint8_t* data, int len) override {
        fprintf(recv_block, "%u %d\n", sequence, pos);

        if (m_active_sequence == -1 || sequence < m_active_sequence || 0 == m_reorders.count(sequence)) {
            std::cout << "wrong sequence " << sequence << ", the active sequence is " << m_active_sequence << std::endl;
            assert(0);
        }

        auto reorder = m_reorders[sequence];
        if (sequence != m_active_sequence) {
            assert(0 == reorder->buffered_data.count(pos));
            reorder->buffered_data[pos] = { data, data + len };
        } else {
            Reorder::BLOCKS blocks;
            auto push_reorder = [&](int32_t p, const uint8_t* d, int l) {
                auto tmp = reorder->reorder->add_block(p, d, l);
                for (auto& t : tmp) {
                    auto r = blocks.insert(t);
                    assert(r.second);
                }
            };

            while (!reorder->buffered_data.empty()) {
                auto itr = reorder->buffered_data.begin();

                push_reorder(itr->first, itr->second.data(), itr->second.size());
                reorder->buffered_data.erase(itr);
            }

            push_reorder(pos, data, len);

            for (auto& block : blocks) {
                fwrite(block.second.data(), 1, block.second.size(), m_out_file);
                fflush(m_out_file);
            }
        }

        if (m_new_line) {
            std::cout << std::string(13, ' ');
        }

        std::cout << "seq " << sequence << " recv[" << recv_packets << "]: " << pos << "(" << len << ")" << std::endl;

        m_new_line = true;
        ++recv_packets;
    }

private:

    struct SequenceData {
        Reorder*             reorder{ nullptr };
        Reorder::BLOCKS      buffered_data;
    };

    bool                                m_new_line{ false };
    BandFecEncoder*                     m_encoder{ nullptr };
    BandFecDecoder*                     m_decoder{ nullptr };
    std::map<uint32_t, SequenceData*>   m_reorders;
    int64_t                             m_active_sequence{ -1 };
    FILE*                               m_out_file{ nullptr };
};

int main() {
    system("chcp 65001");

    const int kReadSize = 1000; ///< kFecParamS
    std::vector<uint8_t> data(kReadSize);

    auto f = fopen("camera.264", "rb");
    fseek(f, 0, SEEK_END);
    auto file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    int in_packets = 0, in_size = 0, progress = -1;

    Foo foo;
    foo.start("recovered.264");
    do {
        if (fread(data.data(), 1, kReadSize, f) < kReadSize) {
            break;
        }

        foo.push_data(data);
        in_size += kReadSize;
#if 0
        if (in_size / kReadSize % 1000 == 0) {
            static int rnd[] = { 5, 10, 15, 20 };
            static int idx = 0;

            auto p = rnd[++idx % (sizeof(rnd) / sizeof(rnd[0]))];
            foo.update(p, p);
        }
#endif
        auto new_progress = (int)(in_size * 100.f / file_size);
        if (new_progress >= progress + 1) {
            SetConsoleTitleA((std::to_string(new_progress) + "%").c_str());
            progress = new_progress;
        }
    } while (true);

    foo.stop();

    in_packets = in_size / kFecParamS;
    fclose(f);

    std::cout << "\n==================================================\n";
    std::cout << "in_packets  : " << in_packets << ", in_size : " << in_size << std::endl;
    std::cout << "out_packets : " << out_packets << ", out_size: " << out_size << std::endl;
    std::cout << "recv_packets: " << recv_packets << std::endl;
    std::cout << "\n--------------------------------------------------\n";
    std::cout << "redundancy: " << (out_size - in_size) * 100. / in_size << "\%" << std::endl;
    std::cout << "recovered: " << num_rejects - (in_packets - recv_packets) << std::endl;

    std::cout << "pass: " << num_pass << ", lost: " << num_rejects << std::endl;
    std::cout << "loss rate before fec: " << std::setprecision(3) << num_rejects * 100. / (num_pass + num_rejects) << "\%" << std::endl;
    std::cout << "loss rate after  fec: " << std::setprecision(3) << (in_packets - recv_packets) * 100. / in_packets << "\%" << std::endl;
}
