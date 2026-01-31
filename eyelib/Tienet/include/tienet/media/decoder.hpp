/**
 * @file decoder.hpp
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <memory>

namespace tienet {
namespace media {

struct DecoderConfig {
	uint32_t width = 0;
	uint32_t height = 0;
	std::string codec = "h264";
};

class IDecoder {
public:
	virtual ~IDecoder() = default;
	virtual bool initialize(const DecoderConfig& cfg) = 0;
	virtual std::vector<uint8_t> decode(const std::vector<uint8_t>& bitstream) = 0;
	virtual void shutdown() = 0;
};

class NullDecoder : public IDecoder {
public:
	bool initialize(const DecoderConfig&) override { return true; }
	std::vector<uint8_t> decode(const std::vector<uint8_t>& bitstream) override { return bitstream; }
	void shutdown() override {}
};

class DecoderFactory {
public:
	static std::unique_ptr<IDecoder> create(const std::string& type);
};

} // namespace media
} // namespace tienet


