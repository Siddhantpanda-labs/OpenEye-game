/**
 * @file advanced_compression.hpp
 * @brief Advanced compression system with LZ4/Zstd support
 */

#pragma once

#include <vector>
#include <memory>
#include <string>
#include <chrono>
#include <unordered_map>
#include <cstdint>

namespace tienet {
namespace core {

/**
 * @brief Compression algorithms
 */
enum class CompressionAlgorithm {
    None,
    LZ4,
    Zstd,
    Adaptive  // Automatically choose best algorithm
};

/**
 * @brief Compression statistics
 */
struct CompressionStats {
    uint64_t bytesCompressed;
    uint64_t bytesDecompressed;
    uint64_t compressionTimeMs;
    uint64_t decompressionTimeMs;
    double averageCompressionRatio;
    uint32_t compressionCount;
    
    CompressionStats() : bytesCompressed(0), bytesDecompressed(0), 
                        compressionTimeMs(0), decompressionTimeMs(0),
                        averageCompressionRatio(0.0), compressionCount(0) {}
};

/**
 * @brief Compression configuration
 */
struct CompressionConfig {
    CompressionAlgorithm algorithm;
    int compressionLevel;        // 1-22 for Zstd, 1-9 for LZ4
    bool enableAdaptiveLevel;   // Automatically adjust compression level
    uint32_t minSizeThreshold; // Minimum size to compress
    uint32_t maxSizeThreshold; // Maximum size to compress
    bool enableDictionary;     // Use dictionary-based compression
    std::string dictionaryPath; // Path to compression dictionary
    
    CompressionConfig() : algorithm(CompressionAlgorithm::LZ4), compressionLevel(6),
                         enableAdaptiveLevel(true), minSizeThreshold(64),
                         maxSizeThreshold(1024 * 1024), enableDictionary(false) {}
};

/**
 * @brief Abstract compression interface
 */
class ICompressor {
public:
    virtual ~ICompressor() = default;
    
    /**
     * @brief Compress data
     * @param input Input data
     * @param output Compressed data
     * @param level Compression level
     * @return true if successful, false otherwise
     */
    virtual bool compress(const std::vector<uint8_t>& input, std::vector<uint8_t>& output, int level) = 0;
    
    /**
     * @brief Decompress data
     * @param input Compressed data
     * @param output Decompressed data
     * @return true if successful, false otherwise
     */
    virtual bool decompress(const std::vector<uint8_t>& input, std::vector<uint8_t>& output) = 0;
    
    /**
     * @brief Get compression algorithm
     * @return Compression algorithm
     */
    virtual CompressionAlgorithm getAlgorithm() const = 0;
    
    /**
     * @brief Get compression statistics
     * @return Compression statistics
     */
    virtual CompressionStats getStats() const = 0;
    
    /**
     * @brief Reset statistics
     */
    virtual void resetStats() = 0;
};

/**
 * @brief LZ4 compressor implementation
 */
class LZ4Compressor : public ICompressor {
public:
    LZ4Compressor();
    ~LZ4Compressor() override;
    
    bool compress(const std::vector<uint8_t>& input, std::vector<uint8_t>& output, int level) override;
    bool decompress(const std::vector<uint8_t>& input, std::vector<uint8_t>& output) override;
    CompressionAlgorithm getAlgorithm() const override { return CompressionAlgorithm::LZ4; }
    CompressionStats getStats() const override { return m_stats; }
    void resetStats() override;

private:
    CompressionStats m_stats;
    std::vector<uint8_t> m_workspace;
    
    void updateStats(uint64_t inputSize, uint64_t outputSize, uint64_t timeMs, bool isCompression);
};

/**
 * @brief Zstd compressor implementation
 */
class ZstdCompressor : public ICompressor {
public:
    ZstdCompressor();
    ~ZstdCompressor() override;
    
    bool compress(const std::vector<uint8_t>& input, std::vector<uint8_t>& output, int level) override;
    bool decompress(const std::vector<uint8_t>& input, std::vector<uint8_t>& output) override;
    CompressionAlgorithm getAlgorithm() const override { return CompressionAlgorithm::Zstd; }
    CompressionStats getStats() const override { return m_stats; }
    void resetStats() override;

private:
    CompressionStats m_stats;
    std::vector<uint8_t> m_workspace;
    void* m_cctx; // Zstd compression context
    void* m_dctx; // Zstd decompression context
    
    void updateStats(uint64_t inputSize, uint64_t outputSize, uint64_t timeMs, bool isCompression);
};

/**
 * @brief Adaptive compressor that chooses best algorithm
 */
class AdaptiveCompressor : public ICompressor {
public:
    AdaptiveCompressor();
    ~AdaptiveCompressor() override;
    
    bool compress(const std::vector<uint8_t>& input, std::vector<uint8_t>& output, int level) override;
    bool decompress(const std::vector<uint8_t>& input, std::vector<uint8_t>& output) override;
    CompressionAlgorithm getAlgorithm() const override { return CompressionAlgorithm::Adaptive; }
    CompressionStats getStats() const override;
    void resetStats() override;
    
    /**
     * @brief Set adaptive parameters
     * @param enableLZ4 Enable LZ4
     * @param enableZstd Enable Zstd
     * @param thresholdSize Size threshold for algorithm selection
     */
    void setAdaptiveParams(bool enableLZ4, bool enableZstd, uint32_t thresholdSize);

private:
    std::unique_ptr<LZ4Compressor> m_lz4Compressor;
    std::unique_ptr<ZstdCompressor> m_zstdCompressor;
    CompressionStats m_stats;
    bool m_enableLZ4;
    bool m_enableZstd;
    uint32_t m_thresholdSize;
    
    CompressionAlgorithm chooseAlgorithm(const std::vector<uint8_t>& input);
};

/**
 * @brief Compression factory
 */
class CompressionFactory {
public:
    /**
     * @brief Create compressor
     * @param config Compression configuration
     * @return Compressor instance
     */
    static std::unique_ptr<ICompressor> createCompressor(const CompressionConfig& config);
    
    /**
     * @brief Get available algorithms
     * @return Vector of available algorithms
     */
    static std::vector<CompressionAlgorithm> getAvailableAlgorithms();
    
    /**
     * @brief Check if algorithm is supported
     * @param algorithm Algorithm to check
     * @return true if supported, false otherwise
     */
    static bool isAlgorithmSupported(CompressionAlgorithm algorithm);
};

/**
 * @brief Dictionary-based compression manager
 */
class CompressionDictionary {
public:
    CompressionDictionary();
    ~CompressionDictionary();
    
    /**
     * @brief Load dictionary from file
     * @param path Dictionary file path
     * @return true if successful, false otherwise
     */
    bool loadFromFile(const std::string& path);
    
    /**
     * @brief Build dictionary from training data
     * @param trainingData Vector of training data samples
     * @param dictSize Target dictionary size
     * @return true if successful, false otherwise
     */
    bool buildFromData(const std::vector<std::vector<uint8_t>>& trainingData, size_t dictSize);
    
    /**
     * @brief Get dictionary data
     * @return Dictionary data
     */
    const std::vector<uint8_t>& getData() const { return m_dictionary; }
    
    /**
     * @brief Check if dictionary is loaded
     * @return true if loaded, false otherwise
     */
    bool isLoaded() const { return !m_dictionary.empty(); }

private:
    std::vector<uint8_t> m_dictionary;
};

} // namespace core
} // namespace tienet
