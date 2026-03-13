#ifndef ___REORDER_H___
#define ___REORDER_H___

#include <map>
#include <vector>

class Reorder {
public:
    typedef std::map<uint32_t, std::vector<uint8_t>> BLOCKS;

    Reorder(uint32_t max_disorder = 6) : m_max_disorder(max_disorder) {

    }

    BLOCKS add_block(uint32_t index, const uint8_t* data, int len) {
        BLOCKS blocks;
        if (index >= m_expecting_index) {
            m_pending_blocks[index] = std::vector<uint8_t>(data, data + len);
        }

        if (!m_pending_blocks.count(m_expecting_index)) {
            if (index - m_expecting_index < m_max_disorder) {
                return {};
            }
        }

        if (m_pending_blocks.empty()) {
            return {};
        }

        m_expecting_index = m_pending_blocks.begin()->first;
        while (m_pending_blocks.count(m_expecting_index)) {
            auto itr = m_pending_blocks.begin();

            blocks.insert(*itr);
            m_pending_blocks.erase(itr);

            ++m_expecting_index;
        }

        return blocks;
    }

    BLOCKS flush() {
        auto blocks = m_pending_blocks;
        m_pending_blocks.clear();

        return blocks;
    }

private:

    const uint32_t      m_max_disorder{ 0 };
    uint32_t            m_expecting_index{ 0 };
    BLOCKS              m_pending_blocks;
};

#endif ///< ___REORDER_H___
