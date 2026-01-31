/**
 * @file compressor.cpp
 * @brief Generic compression implementation
 */

#include "tienet/compression/compressor.hpp"
#include <cstring>

namespace tienet {
namespace compression {

// NoCompressor implementation
std::vector<uint8_t> NoCompressor::compress(const void* data, size_t size, CompressionLevel level) {
    const uint8_t* dataPtr = static_cast<const uint8_t*>(data);
    return std::vector<uint8_t>(dataPtr, dataPtr + size);
}

std::vector<uint8_t> NoCompressor::compress(const std::vector<uint8_t>& data, CompressionLevel level) {
    return data;
}

std::vector<uint8_t> NoCompressor::decompress(const void* data, size_t size, size_t originalSize) {
    const uint8_t* dataPtr = static_cast<const uint8_t*>(data);
    return std::vector<uint8_t>(dataPtr, dataPtr + size);
}

std::vector<uint8_t> NoCompressor::decompress(const std::vector<uint8_t>& data, size_t originalSize) {
    return data;
}

CompressionStats NoCompressor::getStats() const {
    return m_stats;
}

void NoCompressor::resetStats() {
    m_stats = CompressionStats();
}

const char* NoCompressor::getName() const {
    return "none";
}

bool NoCompressor::isAvailable() const {
    return true;
}

// LZ4Compressor implementation
LZ4Compressor::LZ4Compressor() : m_available(false) {
#ifdef TIENET_HAS_LZ4
    m_available = true;
#endif
}

LZ4Compressor::~LZ4Compressor() {
}

std::vector<uint8_t> LZ4Compressor::compress(const void* data, size_t size, CompressionLevel level) {
    if (!m_available) {
        // Fallback to no compression
        const uint8_t* dataPtr = static_cast<const uint8_t*>(data);
        return std::vector<uint8_t>(dataPtr, dataPtr + size);
    }
    
#ifdef TIENET_HAS_LZ4
    auto start = std::chrono::high_resolution_clock::now();
    
    // Estimate compressed size
    int maxCompressedSize = LZ4_compressBound(static_cast<int>(size));
    std::vector<uint8_t> compressed(maxCompressedSize);
    
    int compressedSize = LZ4_compress_fast(
        static_cast<const char*>(data),
        reinterpret_cast<char*>(compressed.data()),
        static_cast<int>(size),
        maxCompressedSize,
        getLZ4Level(level)
    );
    
    if (compressedSize <= 0) {
        // Compression failed, return original data
        const uint8_t* dataPtr = static_cast<const uint8_t*>(data);
        return std::vector<uint8_t>(dataPtr, dataPtr + size);
    }
    
    compressed.resize(compressedSize);
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    // Update statistics
    m_stats.originalSize += size;
    m_stats.compressedSize += compressedSize;
    m_stats.compressionTime += duration.count();
    m_stats.compressionRatio = static_cast<uint32_t>((m_stats.compressedSize * 100) / m_stats.originalSize);
    
    return compressed;
#else
    // Fallback to no compression
    const uint8_t* dataPtr = static_cast<const uint8_t*>(data);
    return std::vector<uint8_t>(dataPtr, dataPtr + size);
#endif
}

std::vector<uint8_t> LZ4Compressor::compress(const std::vector<uint8_t>& data, CompressionLevel level) {
    return compress(data.data(), data.size(), level);
}

std::vector<uint8_t> LZ4Compressor::decompress(const void* data, size_t size, size_t originalSize) {
    if (!m_available) {
        // Fallback to no decompression
        const uint8_t* dataPtr = static_cast<const uint8_t*>(data);
        return std::vector<uint8_t>(dataPtr, dataPtr + size);
    }
    
#ifdef TIENET_HAS_LZ4
    auto start = std::chrono::high_resolution_clock::now();
    
    std::vector<uint8_t> decompressed(originalSize);
    
    int decompressedSize = LZ4_decompress_safe(
        static_cast<const char*>(data),
        reinterpret_cast<char*>(decompressed.data()),
        static_cast<int>(size),
        static_cast<int>(originalSize)
    );
    
    if (decompressedSize <= 0) {
        // Decompression failed
        return std::vector<uint8_t>();
    }
    
    decompressed.resize(decompressedSize);
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    // Update statistics
    m_stats.decompressionTime += duration.count();
    
    return decompressed;
#else
    // Fallback to no decompression
    const uint8_t* dataPtr = static_cast<const uint8_t*>(data);
    return std::vector<uint8_t>(dataPtr, dataPtr + size);
#endif
}

std::vector<uint8_t> LZ4Compressor::decompress(const std::vector<uint8_t>& data, size_t originalSize) {
    return decompress(data.data(), data.size(), originalSize);
}

CompressionStats LZ4Compressor::getStats() const {
    return m_stats;
}

void LZ4Compressor::resetStats() {
    m_stats = CompressionStats();
}

const char* LZ4Compressor::getName() const {
    return "lz4";
}

bool LZ4Compressor::isAvailable() const {
    return m_available;
}

int LZ4Compressor::getLZ4Level(CompressionLevel level) const {
    switch (level) {
        case CompressionLevel::Fast:
            return 1;
        case CompressionLevel::Balanced:
            return 3;
        case CompressionLevel::Best:
            return 9;
        default:
            return 3;
    }
}

// ZstdCompressor implementation
ZstdCompressor::ZstdCompressor() : m_available(false) {
#ifdef TIENET_HAS_ZSTD
    m_available = true;
#endif
}

ZstdCompressor::~ZstdCompressor() {
}

std::vector<uint8_t> ZstdCompressor::compress(const void* data, size_t size, CompressionLevel level) {
    if (!m_available) {
        // Fallback to no compression
        const uint8_t* dataPtr = static_cast<const uint8_t*>(data);
        return std::vector<uint8_t>(dataPtr, dataPtr + size);
    }
    
#ifdef TIENET_HAS_ZSTD
    auto start = std::chrono::high_resolution_clock::now();
    
    // Estimate compressed size
    size_t maxCompressedSize = ZSTD_compressBound(size);
    std::vector<uint8_t> compressed(maxCompressedSize);
    
    size_t compressedSize = ZSTD_compress(
        compressed.data(),
        maxCompressedSize,
        data,
        size,
        getZstdLevel(level)
    );
    
    if (ZSTD_isError(compressedSize)) {
        // Compression failed, return original data
        const uint8_t* dataPtr = static_cast<const uint8_t*>(data);
        return std::vector<uint8_t>(dataPtr, dataPtr + size);
    }
    
    compressed.resize(compressedSize);
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    // Update statistics
    m_stats.originalSize += size;
    m_stats.compressedSize += compressedSize;
    m_stats.compressionTime += duration.count();
    m_stats.compressionRatio = static_cast<uint32_t>((m_stats.compressedSize * 100) / m_stats.originalSize);
    
    return compressed;
#else
    // Fallback to no compression
    const uint8_t* dataPtr = static_cast<const uint8_t*>(data);
    return std::vector<uint8_t>(dataPtr, dataPtr + size);
#endif
}

std::vector<uint8_t> ZstdCompressor::compress(const std::vector<uint8_t>& data, CompressionLevel level) {
    return compress(data.data(), data.size(), level);
}

std::vector<uint8_t> ZstdCompressor::decompress(const void* data, size_t size, size_t originalSize) {
    if (!m_available) {
        // Fallback to no decompression
        const uint8_t* dataPtr = static_cast<const uint8_t*>(data);
        return std::vector<uint8_t>(dataPtr, dataPtr + size);
    }
    
#ifdef TIENET_HAS_ZSTD
    auto start = std::chrono::high_resolution_clock::now();
    
    std::vector<uint8_t> decompressed(originalSize);
    
    size_t decompressedSize = ZSTD_decompress(
        decompressed.data(),
        originalSize,
        data,
        size
    );
    
    if (ZSTD_isError(decompressedSize)) {
        // Decompression failed
        return std::vector<uint8_t>();
    }
    
    decompressed.resize(decompressedSize);
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    // Update statistics
    m_stats.decompressionTime += duration.count();
    
    return decompressed;
#else
    // Fallback to no decompression
    const uint8_t* dataPtr = static_cast<const uint8_t*>(data);
    return std::vector<uint8_t>(dataPtr, dataPtr + size);
#endif
}

std::vector<uint8_t> ZstdCompressor::decompress(const std::vector<uint8_t>& data, size_t originalSize) {
    return decompress(data.data(), data.size(), originalSize);
}

CompressionStats ZstdCompressor::getStats() const {
    return m_stats;
}

void ZstdCompressor::resetStats() {
    m_stats = CompressionStats();
}

const char* ZstdCompressor::getName() const {
    return "zstd";
}

bool ZstdCompressor::isAvailable() const {
    return m_available;
}

int ZstdCompressor::getZstdLevel(CompressionLevel level) const {
    switch (level) {
        case CompressionLevel::Fast:
            return 1;
        case CompressionLevel::Balanced:
            return 3;
        case CompressionLevel::Best:
            return 19;
        default:
            return 3;
    }
}

// CompressorFactory implementation
std::unique_ptr<Compressor> CompressorFactory::createCompressor(const std::string& name) {
    if (name == "none") {
        return createNoCompressor();
    } else if (name == "lz4") {
        return createLZ4Compressor();
    } else if (name == "zstd") {
        return createZstdCompressor();
    }
    
    return createNoCompressor();
}

std::unique_ptr<Compressor> CompressorFactory::createNoCompressor() {
    return std::make_unique<NoCompressor>();
}

std::unique_ptr<Compressor> CompressorFactory::createLZ4Compressor() {
    return std::make_unique<LZ4Compressor>();
}

std::unique_ptr<Compressor> CompressorFactory::createZstdCompressor() {
    return std::make_unique<ZstdCompressor>();
}

std::vector<std::string> CompressorFactory::getAvailableCompressors() {
    std::vector<std::string> compressors;
    
    compressors.push_back("none");
    
#ifdef TIENET_HAS_LZ4
    compressors.push_back("lz4");
#endif
    
#ifdef TIENET_HAS_ZSTD
    compressors.push_back("zstd");
#endif
    
    return compressors;
}

bool CompressorFactory::isCompressorAvailable(const std::string& name) {
    if (name == "none") {
        return true;
    } else if (name == "lz4") {
#ifdef TIENET_HAS_LZ4
        return true;
#else
        return false;
#endif
    } else if (name == "zstd") {
#ifdef TIENET_HAS_ZSTD
        return true;
#else
        return false;
#endif
    }
    
    return false;
}

} // namespace compression
} // namespace tienet
