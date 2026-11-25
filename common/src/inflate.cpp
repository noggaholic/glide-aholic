// Archive-format decoder using Huffman codes and length/distance back-references.
#include "glide/inflate.h"

#include <algorithm>
#include <array>
#include <initializer_list>
#include <vector>

namespace glide {
namespace {
constexpr size_t AlphabetLimit = 285;
constexpr unsigned LookupBits = 8;

// Little-endian words, read from bit 31 down to bit 0. Every 16384th
// physical word is a checksum, outside the compressed bitstream.
class WordBitStream {
  public:
    explicit WordBitStream(ByteView bytes) : bytes_(bytes) {}
    bool Peek(unsigned count, uint32_t& value) {
        while (available_ < count) {
            if (word_ % 0x4000 == 0x3fff)
                ++word_;
            if (word_ >= bytes_.size / 4)
                return false;
            const uint8_t* p = bytes_.data + 4 * word_++;
            const uint32_t next =
                uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
            pending_ = (pending_ << 32) | next;
            available_ += 32;
        }
        value = static_cast<uint32_t>(pending_ >> (available_ - count));
        return true;
    }
    void Consume(unsigned count) {
        available_ -= count;
        pending_ &= (uint64_t(1) << available_) - 1;
    }
    bool Read(unsigned count, uint32_t& value) {
        if (!Peek(count, value))
            return false;
        Consume(count);
        return true;
    }

  private:
    ByteView bytes_;
    size_t word_ = 0;
    uint64_t pending_ = 0;
    unsigned available_ = 0;
};

// Canonical codes descend within each width, in ascending symbol order.
// A binary trie handles long codes; a prefix table resolves short codes directly.
class Codebook {
  public:
    bool Build(const uint8_t* lengths, size_t count) {
        nodes_.clear();
        nodes_.emplace_back();
        std::array<unsigned, 32> widths{};
        for (size_t symbol = 0; symbol < count; ++symbol)
            ++widths[lengths[symbol]];
        if (widths[0] == count) {
            nodes_[0].symbol = static_cast<int>(count - 1);
        } else {
            uint64_t available = 2;
            for (unsigned width = 1; width < widths.size(); ++width) {
                if (widths[width] > available)
                    return false;
                uint64_t code = available - 1;
                for (size_t symbol = 0; symbol < count; ++symbol) {
                    if (lengths[symbol] == width && !Insert(code--, width, static_cast<int>(symbol)))
                        return false;
                }
                available = (available - widths[width]) * 2;
            }
        }
        for (unsigned prefix = 0; prefix < lookup_.size(); ++prefix) {
            Prefix& entry = lookup_[prefix];
            entry.node = 0;
            entry.bits = 0;
            while (entry.bits < LookupBits && entry.node >= 0 && nodes_[entry.node].symbol < 0) {
                const unsigned bit = (prefix >> (LookupBits - 1 - entry.bits)) & 1;
                entry.node = nodes_[entry.node].child[bit];
                ++entry.bits;
            }
        }
        return true;
    }
    bool Decode(WordBitStream& stream, unsigned& symbol) const {
        int node = 0;
        uint32_t prefix;
        if (nodes_[node].symbol < 0 && stream.Peek(LookupBits, prefix)) {
            const Prefix& entry = lookup_[prefix];
            node = entry.node;
            stream.Consume(entry.bits);
        }
        // Near the end, decode only the bits required by this symbol.
        while (node >= 0 && nodes_[node].symbol < 0) {
            uint32_t bit;
            if (!stream.Read(1, bit))
                return false;
            node = nodes_[node].child[bit];
        }
        if (node < 0)
            return false;
        symbol = static_cast<unsigned>(nodes_[node].symbol);
        return true;
    }

  private:
    struct Node {
        int child[2] = {-1, -1};
        int symbol = -1;
    };
    struct Prefix {
        int node = -1;
        unsigned bits = 0;
    };
    bool Insert(uint64_t code, unsigned width, int symbol) {
        int node = 0;
        for (unsigned remaining = width; remaining; --remaining) {
            if (nodes_[node].symbol >= 0)
                return false;
            const unsigned bit = static_cast<unsigned>((code >> (remaining - 1)) & 1);
            int next = nodes_[node].child[bit];
            if (next < 0) {
                next = static_cast<int>(nodes_.size());
                nodes_[node].child[bit] = next;
                nodes_.emplace_back();
            }
            node = next;
        }
        if (nodes_[node].symbol >= 0 || nodes_[node].child[0] >= 0 || nodes_[node].child[1] >= 0)
            return false;
        nodes_[node].symbol = symbol;
        return true;
    }
    std::vector<Node> nodes_;
    std::array<Prefix, 1u << LookupBits> lookup_{};
};

const Codebook& HeaderCodebook() {
    static const Codebook dictionary = [] {
        // Fixed format constants: unlisted descriptors have 16-bit codes.
        struct WidthGroup {
            uint8_t width;
            std::initializer_list<unsigned> symbols;
        };
        const WidthGroup groups[] = {{3, {8, 9, 10}},
                                     {4, {0, 7, 11, 12}},
                                     {5, {6, 41, 42, 224}},
                                     {6, {4, 5, 32, 40, 43, 44, 64, 74}},
                                     {7, {3, 13, 37, 38, 39, 72, 73}},
                                     {8, {36, 71, 75, 76, 105, 106}},
                                     {9, {35, 70, 96, 99, 103, 104, 136, 137, 160, 232}},
                                     {10, {1, 2, 45, 67, 68, 69, 101, 102, 128, 135, 138, 168, 169, 192, 201, 233}},
                                     {11, {14, 77, 100, 107, 108, 132, 133, 139, 164, 165, 170, 200, 229}},
                                     {12, {131, 134, 166, 167, 199, 202, 231}},
                                     {13, {34, 46, 140, 196, 228, 230}},
                                     {14, {78, 109, 198, 236}},
                                     {15, {15, 16, 17, 141, 171, 172, 204, 234}}};
        std::array<uint8_t, 256> lengths;
        lengths.fill(16);
        for (const WidthGroup& group : groups)
            for (unsigned symbol : group.symbols)
                lengths[symbol] = group.width;
        Codebook result;
        result.Build(lengths.data(), lengths.size());
        return result;
    }();
    return dictionary;
}

bool ReadCodebook(WordBitStream& stream, Codebook& book) {
    uint32_t remaining;
    if (!stream.Read(16, remaining) || !remaining || remaining > AlphabetLimit)
        return false;
    const size_t count = remaining;
    std::array<uint8_t, AlphabetLimit> lengths{};
    while (remaining) {
        unsigned descriptor;
        if (!HeaderCodebook().Decode(stream, descriptor))
            return false;
        const unsigned run = 1 + (descriptor >> 5);
        if (run > remaining)
            return false;
        const uint8_t width = static_cast<uint8_t>(descriptor & 31);
        std::fill(lengths.begin() + remaining - run, lengths.begin() + remaining, width);
        remaining -= run;
    }
    return book.Build(lengths.data(), count);
}

bool ReadLength(WordBitStream& stream, unsigned code, unsigned adjustment, size_t& length) {
    if (code == 28) {
        length = 255 + adjustment;
        return true;
    }
    const unsigned group = code / 4;
    uint32_t extra = 0;
    if (group > 6 || (group > 1 && !stream.Read(group - 1, extra)))
        return false;
    length = (group ? size_t(4 + code % 4) << (group - 1) : code) + extra + adjustment;
    return true;
}

bool ReadDistance(WordBitStream& stream, unsigned code, size_t& distance) {
    const unsigned group = code / 2;
    uint32_t extra = 0;
    if (group > 16 || (group > 1 && !stream.Read(group - 1, extra)))
        return false;
    distance = (group ? size_t(2 + code % 2) << (group - 1) : code) + extra + 1;
    return true;
}
} // namespace

bool InflateArchive(ByteView input, Buffer& output, size_t maximum, size_t prefix) {
    if (!input.Valid() || input.size < 12)
        return false;
    WordBitStream stream(input);
    uint32_t ignored, declared, control;
    if (!stream.Read(32, ignored) || !stream.Read(32, declared) || !declared || declared > maximum ||
        !stream.Read(8, control))
        return false;
    const unsigned adjustment = (control & 15) + 1;
    const size_t wanted = prefix ? (std::min)(prefix, size_t(declared)) : declared;
    Buffer decoded;
    if (!decoded.Resize(wanted))
        return false;
    size_t written = 0;
    Codebook literals, distances;
    while (written < wanted) {
        uint32_t budget;
        if (!ReadCodebook(stream, literals) || !ReadCodebook(stream, distances) || !stream.Read(4, budget))
            return false;
        const unsigned tokens = (budget + 1) * 4096;
        for (unsigned token = 0; token < tokens && written < wanted; ++token) {
            unsigned symbol;
            if (!literals.Decode(stream, symbol))
                return false;
            if (symbol < 256) {
                decoded.Data()[written++] = static_cast<uint8_t>(symbol);
                continue;
            }
            size_t length, distance;
            if (!ReadLength(stream, symbol - 256, adjustment, length) || !distances.Decode(stream, symbol) ||
                !ReadDistance(stream, symbol, distance) || distance > written)
                return false;
            // Copies may overlap, and the requested prefix may end inside a copy.
            const size_t end = written + (std::min)(length, wanted - written);
            while (written < end) {
                decoded.Data()[written] = decoded.Data()[written - distance];
                ++written;
            }
        }
    }
    output.Swap(decoded);
    return true;
}
} // namespace glide
