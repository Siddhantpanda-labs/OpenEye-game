#pragma once

#include <cstdint>
#include <vector>

namespace winclient {

// Decode frame to RGB8 (width * height * 3, row-major).
// format 0 = JPEG, 1 = H.264.
class FrameDecoder {
public:
  FrameDecoder() = default;
  ~FrameDecoder();

  // Decode one frame. Returns true if output is valid. outRgb may be resized.
  // If optional needMoreInput is set and decode returns false, decoder needs more NALs (e.g. SPS/PPS first) - not an error.
  bool decode(uint8_t format, const uint8_t* data, size_t size,
              std::vector<uint8_t>& outRgb, int& outWidth, int& outHeight,
              bool* needMoreInput = nullptr);

  void reset(); // Release internal state (e.g. MF decoder)

private:
  bool decodeJpeg(const uint8_t* data, size_t size,
                  std::vector<uint8_t>& outRgb, int& outWidth, int& outHeight);
  bool decodeH264(const uint8_t* data, size_t size,
                  std::vector<uint8_t>& outRgb, int& outWidth, int& outHeight,
                  bool* needMoreInput);

  void* mfDecoder_{nullptr}; // IMFTransform* + state
  int lastWidth_{0};
  int lastHeight_{0};
};

} // namespace winclient
