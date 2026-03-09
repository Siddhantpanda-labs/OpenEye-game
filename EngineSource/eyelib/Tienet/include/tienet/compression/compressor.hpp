/**
 * @file compressor.hpp
 * @brief Generic compression interface for network data
 */

#pragma once

#include <vector>
#include <cstdint>
#include <memory>
#include <string>

namespace tienet {
namespace compression {

/**
 * @brief Compression levels
 */
enum class CompressionLevel {
    None = 0,
    Fast = 1,
    Balanced = 2,
    Best = 3
};

/**
 * @brief Compression statistics
 */
struct CompressionStats {
    uint64_t originalSize;
    uint64_t compressedSize;
    uint64_t compressionTime;
    uint64_t decompressionTime;
    uint32_t compressionRatio;
    
    CompressionStats() : originalSize(0), compressedSize(0), compressionTime(0),
                        decompressionTime(0), compressionRatio(0) {}
};

/**
 * @brief Abstract compressor interface
 */
class Compressor {
public:
    virtual ~Compressor() = default;
    
    /**
     * @brief Compress data
     * @param data Data to compress
     * @param size Data size
     * @param level Compression level
     * @return Compressed data
     */
    virtual std::vector<uint8_t> compress(const void* data, size_t size, CompressionLevel level = CompressionLevel::Balanced) = 0;
    
    /**
     * @brief Compress data
     * @param data Data to compress
     * @param level Compression level
     * @return Compressed data
     */
    virtual std::vector<uint8_t> compress(const std::vector<uint8_t>& data, CompressionLevel level = CompressionLevel::Balanced) = 0;
    
    /**
     * @brief Decompress data
     * @param data Compressed data
     * @param size Data size
     * @param originalSize Original data size
     * @return Decompressed data
     */
    virtual std::vector<uint8_t> decompress(const void* data, size_t size, size_t originalSize) = 0;
    
    /**
     * @brief Decompress data
     * @param data Compressed data
     * @param originalSize Original data size
     * @return Decompressed data
     */
    virtual std::vector<uint8_t> decompress(const std::vector<uint8_t>& data, size_t originalSize) = 0;
    
    /**
     * @brief Get compression statistics
     * @return Compression statistics
     */
    virtual CompressionStats getStats() const = 0;
    
    /**
     * @brief Reset compression statistics
     */
    virtual void resetStats() = 0;
    
    /**
     * @brief Get compressor name
     * @return Compressor name
     */
    virtual const char* getName() const = 0;
    
    /**
     * @brief Check if compressor is available
     * @return true if available
     */
    virtual bool isAvailable() const = 0;
};

/**
 * @brief No-op compressor (pass-through)
 */
class NoCompressor : public Compressor {
public:
    virtual ~NoCompressor() = default;
    
    virtual std::vector<uint8_t> compress(const void* data, size_t size, CompressionLevel level = CompressionLevel::Balanced) override;
    virtual std::vector<uint8_t> compress(const std::vector<uint8_t>& data, CompressionLevel level = CompressionLevel::Balanced) override;
    virtual std::vector<uint8_t> decompress(const void* data, size_t size, size_t originalSize) override;
    virtual std::vector<uint8_t> decompress(const std::vector<uint8_t>& data, size_t originalSize) override;
    virtual CompressionStats getStats() const override;
    virtual void resetStats() override;
    virtual const char* getName() const override;
    virtual bool isAvailable() const override;

private:
    mutable CompressionStats m_stats;
};

/**
 * @brief LZ4 compressor
 */
class LZ4Compressor : public Compressor {
public:
    LZ4Compressor();
    virtual ~LZ4Compressor();
    
    virtual std::vector<uint8_t> compress(const void* data, size_t size, CompressionLevel level = CompressionLevel::Balanced) override;
    virtual std::vector<uint8_t> compress(const std::vector<uint8_t>& data, CompressionLevel level = CompressionLevel::Balanced) override;
    virtual std::vector<uint8_t> decompress(const void* data, size_t size, size_t originalSize) override;
    virtual std::vector<uint8_t> decompress(const std::vector<uint8_t>& data, size_t originalSize) override;
    virtual CompressionStats getStats() const override;
    virtual void resetStats() override;
    virtual const char* getName() const override;
    virtual bool isAvailable() const override;

private:
    mutable CompressionStats m_stats;
    bool m_available;
    
    int getLZ4Level(CompressionLevel level) const;
};

/**
 * @brief Zstandard compressor
 */
class ZstdCompressor : public Compressor {
public:
    ZstdCompressor();
    virtual ~ZstdCompressor();
    
    virtual std::vector<uint8_t> compress(const void* data, size_t size, CompressionLevel level = CompressionLevel::Balanced) override;
    virtual std::vector<uint8_t> compress(const std::vector<uint8_t>& data, CompressionLevel level = CompressionLevel::Balanced) override;
    virtual std::vector<uint8_t> decompress(const void* data, size_t size, size_t originalSize) override;
    virtual std::vector<uint8_t> decompress(const std::vector<uint8_t>& data, size_t originalSize) override;
    virtual CompressionStats getStats() const override;
    virtual void resetStats() override;
    virtual const char* getName() const override;
    virtual bool isAvailable() const override;

private:
    mutable CompressionStats m_stats;
    bool m_available;
    
    int getZstdLevel(CompressionLevel level) const;
};

/**
 * @brief Compressor factory
 */
class CompressorFactory {
public:
    /**
     * @brief Create compressor by name
     * @param name Compressor name ("none", "lz4", "zstd")
     * @return Compressor instance
     */
    static std::unique_ptr<Compressor> createCompressor(const std::string& name);
    
    /**
     * @brief Create no-op compressor
     * @return No-op compressor
     */
    static std::unique_ptr<Compressor> createNoCompressor();
    
    /**
     * @brief Create LZ4 compressor
     * @return LZ4 compressor
     */
    static std::unique_ptr<Compressor> createLZ4Compressor();
    
    /**
     * @brief Create Zstandard compressor
     * @return Zstandard compressor
     */
    static std::unique_ptr<Compressor> createZstdCompressor();
    
    /**
     * @brief Get available compressors
     * @return Vector of available compressor names
     */
    static std::vector<std::string> getAvailableCompressors();
    
    /**
     * @brief Check if compressor is available
     * @param name Compressor name
     * @return true if available
     */
    static bool isCompressorAvailable(const std::string& name);
};

} // namespace compression
} // namespace tienet
