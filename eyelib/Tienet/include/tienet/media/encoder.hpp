/**
 * @file encoder.hpp
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <memory>

namespace tienet {
namespace media {

struct EncoderConfig {
	uint32_t width = 0;
	uint32_t height = 0;
	uint32_t bitrateBps = 4 * 1024 * 1024;
	uint32_t fps = 60;
	std::string codec = "h264";
    uint32_t keyframeIntervalMs = 1000; // force IDR at interval
};

class IEncoder {
public:
	virtual ~IEncoder() = default;
	virtual bool initialize(const EncoderConfig& cfg) = 0;
	virtual std::vector<uint8_t> encode(const std::vector<uint8_t>& rgbaFrame) = 0;
	virtual void shutdown() = 0;
    virtual bool reconfigure(const EncoderConfig& cfg) = 0;
    virtual void forceKeyframe() = 0;
#if defined(TIENET_HAS_CUDA)
    // Optional CUDA path for zero-copy (NV12 planar device pointer input)
    virtual std::vector<uint8_t> encodeCudaNV12(void* cudaDevicePtrY, void* cudaDevicePtrUV, uint32_t pitch) { (void)cudaDevicePtrY; (void)cudaDevicePtrUV; (void)pitch; return {}; }
#endif
};

class NullEncoder : public IEncoder {
public:
	bool initialize(const EncoderConfig&) override { return true; }
	std::vector<uint8_t> encode(const std::vector<uint8_t>& rgbaFrame) override { return rgbaFrame; }
	void shutdown() override {}
    bool reconfigure(const EncoderConfig&) override { return true; }
    void forceKeyframe() override {}
};

class EncoderFactory {
public:
	static std::unique_ptr<IEncoder> create(const std::string& type);
};

} // namespace media
} // namespace tienet


