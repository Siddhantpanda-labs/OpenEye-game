/**
 * @file advanced_compression.cpp
 * @brief Advanced compression system implementation
 */

#include "tienet/core/advanced_compression.hpp"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>

// LZ4 includes
#ifdef TIENET_HAS_LZ4
#include <lz4.h>
#include <lz4hc.h>
#endif

// Zstd includes
#ifdef TIENET_HAS_ZSTD
#include <zstd.h>
#endif

namespace tienet {
namespace core {

// LZ4 Compressor implementation
LZ4Compressor::LZ4Compressor() {
#ifdef TIENET_HAS_LZ4
    m_workspace.resize(LZ4_COMPRESSBOUND(1024 * 1024)); // Reserve space for 1MB input
#else
    m_workspace.clear();
#endif
}

LZ4Compressor::~LZ4Compressor() {
}

bool LZ4Compressor::compress(const std::vector<uint8_t>& input, std::vector<uint8_t>& output, int level) {
#ifdef TIENET_HAS_LZ4
    if (input.empty()) {
        output.clear();
        return true;
    }
    
    auto start = std::chrono::high_resolution_clock::now();
    
    // Ensure workspace is large enough
    int maxCompressedSize = LZ4_COMPRESSBOUND(static_cast<int>(input.size()));
    if (static_cast<size_t>(maxCompressedSize) > m_workspace.size()) {
        m_workspace.resize(maxCompressedSize);
    }
    
    int compressedSize;
    if (level > 3) {
        // Use LZ4HC for higher compression levels
        compressedSize = LZ4_compress_HC(
            reinterpret_cast<const char*>(input.data()),
            reinterpret_cast<char*>(m_workspace.data()),
            static_cast<int>(input.size()),
            maxCompressedSize,
            std::min(level, 16)
        );
    } else {
        // Use fast LZ4 for lower compression levels
        compressedSize = LZ4_compress_fast(
            reinterpret_cast<const char*>(input.data()),
            reinterpret_cast<char*>(m_workspace.data()),
            static_cast<int>(input.size()),
            maxCompressedSize,
            level
        );
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    if (compressedSize > 0) {
        output.assign(m_workspace.begin(), m_workspace.begin() + compressedSize);
        updateStats(input.size(), output.size(), duration.count() / 1000, true);
        return true;
    }
    
    return false;
#else
    // Fallback: no compression
    output = input;
    return true;
#endif
}

bool LZ4Compressor::decompress(const std::vector<uint8_t>& input, std::vector<uint8_t>& output) {
#ifdef TIENET_HAS_LZ4
    if (input.empty()) {
        output.clear();
        return true;
    }
    
    auto start = std::chrono::high_resolution_clock::now();
    
    // Estimate decompressed size (this is a limitation of LZ4)
    // In practice, you'd store the original size in the compressed data
    size_t estimatedSize = input.size() * 4; // Conservative estimate
    output.resize(estimatedSize);
    
    int decompressedSize = LZ4_decompress_safe(
        reinterpret_cast<const char*>(input.data()),
        reinterpret_cast<char*>(output.data()),
        static_cast<int>(input.size()),
        static_cast<int>(estimatedSize)
    );
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    if (decompressedSize > 0) {
        output.resize(decompressedSize);
        updateStats(output.size(), input.size(), duration.count() / 1000, false);
        return true;
    }
    
    return false;
#else
    // Fallback: no decompression
    output = input;
    return true;
#endif
}

void LZ4Compressor::updateStats(uint64_t inputSize, uint64_t outputSize, uint64_t timeMs, bool isCompression) {
    if (isCompression) {
        m_stats.bytesCompressed += inputSize;
        m_stats.compressionTimeMs += timeMs;
        m_stats.compressionCount++;
        
        // Update average compression ratio
        double ratio = static_cast<double>(outputSize) / inputSize;
        m_stats.averageCompressionRatio = 
            (m_stats.averageCompressionRatio * (m_stats.compressionCount - 1) + ratio) / m_stats.compressionCount;
    } else {
        m_stats.bytesDecompressed += outputSize;
        m_stats.decompressionTimeMs += timeMs;
    }
}

void LZ4Compressor::resetStats() {
    m_stats = CompressionStats();
}

// Zstd Compressor implementation
ZstdCompressor::ZstdCompressor() : m_cctx(nullptr), m_dctx(nullptr) {
#ifdef TIENET_HAS_ZSTD
    m_cctx = ZSTD_createCCtx();
    m_dctx = ZSTD_createDCtx();
    m_workspace.resize(ZSTD_COMPRESSBOUND(1024 * 1024));
#endif
}

ZstdCompressor::~ZstdCompressor() {
#ifdef TIENET_HAS_ZSTD
    if (m_cctx) ZSTD_freeCCtx(m_cctx);
    if (m_dctx) ZSTD_freeDCtx(m_dctx);
#endif
}

bool ZstdCompressor::compress(const std::vector<uint8_t>& input, std::vector<uint8_t>& output, int level) {
#ifdef TIENET_HAS_ZSTD
    if (input.empty()) {
        output.clear();
        return true;
    }
    
    auto start = std::chrono::high_resolution_clock::now();
    
    // Ensure workspace is large enough
    size_t maxCompressedSize = ZSTD_compressBound(input.size());
    if (maxCompressedSize > m_workspace.size()) {
        m_workspace.resize(maxCompressedSize);
    }
    
    size_t compressedSize = ZSTD_compressCCtx(
        m_cctx,
        m_workspace.data(),
        maxCompressedSize,
        input.data(),
        input.size(),
        std::min(level, 22)
    );
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    if (!ZSTD_isError(compressedSize)) {
        output.assign(m_workspace.begin(), m_workspace.begin() + compressedSize);
        updateStats(input.size(), output.size(), duration.count() / 1000, true);
        return true;
    }
    
    return false;
#else
    // Fallback: no compression
    output = input;
    return true;
#endif
}

bool ZstdCompressor::decompress(const std::vector<uint8_t>& input, std::vector<uint8_t>& output) {
#ifdef TIENET_HAS_ZSTD
    if (input.empty()) {
        output.clear();
        return true;
    }
    
    auto start = std::chrono::high_resolution_clock::now();
    
    // Get decompressed size from frame header
    unsigned long long decompressedSize = ZSTD_getFrameContentSize(input.data(), input.size());
    if (decompressedSize == ZSTD_CONTENTSIZE_ERROR || decompressedSize == ZSTD_CONTENTSIZE_UNKNOWN) {
        return false;
    }
    
    output.resize(decompressedSize);
    
    size_t actualSize = ZSTD_decompressDCtx(
        m_dctx,
        output.data(),
        decompressedSize,
        input.data(),
        input.size()
    );
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    if (!ZSTD_isError(actualSize)) {
        output.resize(actualSize);
        updateStats(output.size(), input.size(), duration.count() / 1000, false);
        return true;
    }
    
    return false;
#else
    // Fallback: no decompression
    output = input;
    return true;
#endif
}

void ZstdCompressor::updateStats(uint64_t inputSize, uint64_t outputSize, uint64_t timeMs, bool isCompression) {
    if (isCompression) {
        m_stats.bytesCompressed += inputSize;
        m_stats.compressionTimeMs += timeMs;
        m_stats.compressionCount++;
        
        // Update average compression ratio
        double ratio = static_cast<double>(outputSize) / inputSize;
        m_stats.averageCompressionRatio = 
            (m_stats.averageCompressionRatio * (m_stats.compressionCount - 1) + ratio) / m_stats.compressionCount;
    } else {
        m_stats.bytesDecompressed += outputSize;
        m_stats.decompressionTimeMs += timeMs;
    }
}

void ZstdCompressor::resetStats() {
    m_stats = CompressionStats();
}

// Adaptive Compressor implementation
AdaptiveCompressor::AdaptiveCompressor() 
    : m_enableLZ4(true), m_enableZstd(true), m_thresholdSize(1024) {
    m_lz4Compressor = std::make_unique<LZ4Compressor>();
    m_zstdCompressor = std::make_unique<ZstdCompressor>();
}

AdaptiveCompressor::~AdaptiveCompressor() {
}

bool AdaptiveCompressor::compress(const std::vector<uint8_t>& input, std::vector<uint8_t>& output, int level) {
    CompressionAlgorithm algorithm = chooseAlgorithm(input);
    
    switch (algorithm) {
        case CompressionAlgorithm::LZ4:
            return m_lz4Compressor->compress(input, output, level);
        case CompressionAlgorithm::Zstd:
            return m_zstdCompressor->compress(input, output, level);
        default:
            output = input;
            return true;
    }
}

bool AdaptiveCompressor::decompress(const std::vector<uint8_t>& input, std::vector<uint8_t>& output) {
    // Try Zstd first (it has frame headers)
    if (m_enableZstd && m_zstdCompressor->decompress(input, output)) {
        return true;
    }
    
    // Fall back to LZ4
    if (m_enableLZ4 && m_lz4Compressor->decompress(input, output)) {
        return true;
    }
    
    // No compression
    output = input;
    return true;
}

CompressionStats AdaptiveCompressor::getStats() const {
    CompressionStats combined;
    auto lz4Stats = m_lz4Compressor->getStats();
    auto zstdStats = m_zstdCompressor->getStats();
    
    combined.bytesCompressed = lz4Stats.bytesCompressed + zstdStats.bytesCompressed;
    combined.bytesDecompressed = lz4Stats.bytesDecompressed + zstdStats.bytesDecompressed;
    combined.compressionTimeMs = lz4Stats.compressionTimeMs + zstdStats.compressionTimeMs;
    combined.decompressionTimeMs = lz4Stats.decompressionTimeMs + zstdStats.decompressionTimeMs;
    combined.compressionCount = lz4Stats.compressionCount + zstdStats.compressionCount;
    
    if (combined.compressionCount > 0) {
        combined.averageCompressionRatio = 
            (lz4Stats.averageCompressionRatio * lz4Stats.compressionCount + 
             zstdStats.averageCompressionRatio * zstdStats.compressionCount) / combined.compressionCount;
    }
    
    return combined;
}

void AdaptiveCompressor::resetStats() {
    m_lz4Compressor->resetStats();
    m_zstdCompressor->resetStats();
}

void AdaptiveCompressor::setAdaptiveParams(bool enableLZ4, bool enableZstd, uint32_t thresholdSize) {
    m_enableLZ4 = enableLZ4;
    m_enableZstd = enableZstd;
    m_thresholdSize = thresholdSize;
}

CompressionAlgorithm AdaptiveCompressor::chooseAlgorithm(const std::vector<uint8_t>& input) {
    if (input.size() < m_thresholdSize) {
        return CompressionAlgorithm::LZ4; // LZ4 is faster for small data
    }
    
    if (m_enableZstd) {
        return CompressionAlgorithm::Zstd; // Zstd is better for larger data
    }
    
    return CompressionAlgorithm::LZ4;
}

// Compression Factory implementation
std::unique_ptr<ICompressor> CompressionFactory::createCompressor(const CompressionConfig& config) {
    switch (config.algorithm) {
        case CompressionAlgorithm::LZ4:
            return std::make_unique<LZ4Compressor>();
        case CompressionAlgorithm::Zstd:
            return std::make_unique<ZstdCompressor>();
        case CompressionAlgorithm::Adaptive:
            return std::make_unique<AdaptiveCompressor>();
        default:
            return nullptr;
    }
}

std::vector<CompressionAlgorithm> CompressionFactory::getAvailableAlgorithms() {
    std::vector<CompressionAlgorithm> algorithms;
    
    algorithms.push_back(CompressionAlgorithm::None);
    
#ifdef TIENET_HAS_LZ4
    algorithms.push_back(CompressionAlgorithm::LZ4);
#endif

#ifdef TIENET_HAS_ZSTD
    algorithms.push_back(CompressionAlgorithm::Zstd);
#endif

#ifdef TIENET_HAS_LZ4
#ifdef TIENET_HAS_ZSTD
    algorithms.push_back(CompressionAlgorithm::Adaptive);
#endif
#endif

    return algorithms;
}

bool CompressionFactory::isAlgorithmSupported(CompressionAlgorithm algorithm) {
    switch (algorithm) {
        case CompressionAlgorithm::None:
            return true;
        case CompressionAlgorithm::LZ4:
#ifdef TIENET_HAS_LZ4
            return true;
#else
            return false;
#endif
        case CompressionAlgorithm::Zstd:
#ifdef TIENET_HAS_ZSTD
            return true;
#else
            return false;
#endif
        case CompressionAlgorithm::Adaptive:
#ifdef TIENET_HAS_LZ4
#ifdef TIENET_HAS_ZSTD
            return true;
#else
            return false;
#endif
#else
            return false;
#endif
        default:
            return false;
    }
}

// Compression Dictionary implementation
CompressionDictionary::CompressionDictionary() {
}

CompressionDictionary::~CompressionDictionary() {
}

bool CompressionDictionary::loadFromFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }
    
    file.seekg(0, std::ios::end);
    size_t size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    m_dictionary.resize(size);
    file.read(reinterpret_cast<char*>(m_dictionary.data()), size);
    
    return file.good();
}

bool CompressionDictionary::buildFromData(const std::vector<std::vector<uint8_t>>& trainingData, size_t dictSize) {
#ifdef TIENET_HAS_ZSTD
    if (trainingData.empty()) {
        return false;
    }
    
    // Prepare training data for Zstd
    std::vector<const void*> samples;
    std::vector<size_t> sampleSizes;
    
    for (const auto& sample : trainingData) {
        samples.push_back(sample.data());
        sampleSizes.push_back(sample.size());
    }
    
    m_dictionary.resize(dictSize);
    
    size_t actualSize = ZSTD_trainFromBuffer(
        m_dictionary.data(),
        dictSize,
        samples.data(),
        sampleSizes.data(),
        static_cast<unsigned>(trainingData.size())
    );
    
    if (ZSTD_isError(actualSize)) {
        return false;
    }
    
    m_dictionary.resize(actualSize);
    return true;
#else
    // Fallback: create a simple dictionary from concatenated samples
    m_dictionary.clear();
    for (const auto& sample : trainingData) {
        m_dictionary.insert(m_dictionary.end(), sample.begin(), sample.end());
    }
    
    if (m_dictionary.size() > dictSize) {
        m_dictionary.resize(dictSize);
    }
    
    return true;
#endif
}

} // namespace core
} // namespace tienet
