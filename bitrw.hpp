#include <cstdint>
#include <type_traits>

struct BitWriter {
    uint8_t *data;
    size_t cursor;

    template<typename T>
    void write(T y, size_t n_bits = sizeof(std::decay_t<T>) * 8) {
        using U = std::make_unsigned_t<std::decay_t<T>>;
        U x = static_cast<U>(y);

        while (n_bits) {
            size_t byte_idx = cursor / 8, bit_idx = cursor % 8;
            data[byte_idx] |= (x & 1) << bit_idx;
            x >>= 1;
            --n_bits;
            ++cursor;
        }
    }
};

struct BitReader {
    const uint8_t *data;
    size_t cursor;

    template<typename T>
    T read(size_t n_bits) {
        using U = std::make_unsigned_t<std::decay_t<T>>;
        U x = 0;

        for (size_t i = 0; i < n_bits; ++i)  {
            size_t byte_idx = cursor / 8, bit_idx = cursor % 8;
            x |= ((data[byte_idx] >> bit_idx) & 1) << i;
            ++cursor;
        }

        return x;
    }
};

