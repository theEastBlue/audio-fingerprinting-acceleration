#include <cstdint>
#include <cstring>
#include <string>
#include <sstream>
#include <iomanip>

class SHA1 {
public:
    SHA1() { reset(); }

    void reset() {
        m_total[0] = 0;
        m_total[1] = 0;

        m_state[0] = 0x67452301;
        m_state[1] = 0xEFCDAB89;
        m_state[2] = 0x98BADCFE;
        m_state[3] = 0x10325476;
        m_state[4] = 0xC3D2E1F0;

        memset(m_buffer, 0, 64);
        m_bufferSize = 0;
    }

    void processBytes(const void* data, size_t len) {
        const uint8_t* input = static_cast<const uint8_t*>(data);

        while (len > 0) {
            size_t space = 64 - m_bufferSize;
            size_t take = (len < space) ? len : space;

            memcpy(m_buffer + m_bufferSize, input, take);

            m_bufferSize += take;
            input += take;
            len -= take;

            if (m_bufferSize == 64) {
                processBlock(m_buffer);
                addBytesProcessed(64);
                m_bufferSize = 0;
            }
        }
    }

    std::string getHash() {
        uint64_t totalBits = (m_total[0] << 3);

        uint8_t padding[64] = {0x80};
        size_t padLen = (m_bufferSize < 56) ? (56 - m_bufferSize)
                                            : (120 - m_bufferSize);

        processBytes(padding, padLen);

        uint8_t length[8];
        for (int i = 0; i < 8; i++) {
            length[7 - i] = (totalBits >> (i * 8)) & 0xFF;
        }

        processBytes(length, 8);

        std::ostringstream out;
        out << std::hex << std::setfill('0');

        for (int i = 0; i < 5; i++) {
            out << std::setw(8) << m_state[i];
        }

        reset();
        return out.str();
    }

private:
    uint32_t m_state[5];
    uint64_t m_total[2];
    uint8_t m_buffer[64];
    size_t m_bufferSize;

    static uint32_t rol(uint32_t value, size_t bits) {
        return (value << bits) | (value >> (32 - bits));
    }

    void processBlock(const uint8_t block[64]) {
        uint32_t w[80];

        for (int i = 0; i < 16; i++) {
            w[i] =
                (block[i * 4 + 0] << 24) |
                (block[i * 4 + 1] << 16) |
                (block[i * 4 + 2] << 8) |
                (block[i * 4 + 3]);
        }

        for (int i = 16; i < 80; i++) {
            w[i] = rol(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
        }

        uint32_t a = m_state[0];
        uint32_t b = m_state[1];
        uint32_t c = m_state[2];
        uint32_t d = m_state[3];
        uint32_t e = m_state[4];

        for (int i = 0; i < 80; i++) {
            uint32_t f, k;

            if (i < 20) {
                f = (b & c) | ((~b) & d);
                k = 0x5A827999;
            } else if (i < 40) {
                f = b ^ c ^ d;
                k = 0x6ED9EBA1;
            } else if (i < 60) {
                f = (b & c) | (b & d) | (c & d);
                k = 0x8F1BBCDC;
            } else {
                f = b ^ c ^ d;
                k = 0xCA62C1D6;
            }

            uint32_t temp = rol(a, 5) + f + e + k + w[i];
            e = d;
            d = c;
            c = rol(b, 30);
            b = a;
            a = temp;
        }

        m_state[0] += a;
        m_state[1] += b;
        m_state[2] += c;
        m_state[3] += d;
        m_state[4] += e;
    }

    void addBytesProcessed(size_t n) {
        m_total[0] += n;
        if (m_total[0] < n)
            m_total[1]++;
    }
};