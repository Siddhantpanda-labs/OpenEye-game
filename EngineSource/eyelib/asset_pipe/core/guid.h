#pragma once

#include <cstdint>
#include <string>

namespace asset_pipe {

/** 128-bit GUID for assets. String form: "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx". */
struct Guid {
    uint64_t high = 0;
    uint64_t low = 0;

    static Guid Generate();
    /** Parse from dashed (36-char) or 32-char hex string. */
    static Guid FromString(const std::string& s);
    std::string ToString() const;

    bool IsValid() const;
    static Guid Invalid();

    bool operator==(const Guid& other) const { return high == other.high && low == other.low; }
    bool operator!=(const Guid& other) const { return !(*this == other); }
    bool operator<(const Guid& other) const {
        if (high != other.high) return high < other.high;
        return low < other.low;
    }

    struct Hash {
        size_t operator()(const Guid& g) const;
    };
};

} // namespace asset_pipe
