#include "FrameDecoder.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#ifdef _WIN32
#include <mfapi.h>
#include <mfidl.h>
#include <mftransform.h>
#include <mferror.h>
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfuuid.lib")
#endif

#include <cstring>
#include <cstdio>
#include <vector>

namespace winclient {

#ifdef _WIN32
struct MFDecoderState {
  IMFTransform* mft{nullptr};
  IMFMediaType* inputType{nullptr};
  IMFMediaType* outputType{nullptr};
  int width{0};
  int height{0};
  LONGLONG sampleTime{0};
  LONGLONG sampleDuration{111111}; // ~90 FPS
};
#endif

FrameDecoder::~FrameDecoder() {
  reset();
}

void FrameDecoder::reset() {
#ifdef _WIN32
  auto* s = (MFDecoderState*)mfDecoder_;
  if (s) {
    if (s->outputType) s->outputType->Release();
    if (s->inputType) s->inputType->Release();
    if (s->mft) s->mft->Release();
    delete s;
    mfDecoder_ = nullptr;
  }
#endif
  lastWidth_ = 0;
  lastHeight_ = 0;
}

bool FrameDecoder::decodeJpeg(const uint8_t* data, size_t size,
                              std::vector<uint8_t>& outRgb, int& outWidth, int& outHeight) {
  int w = 0, h = 0, ch = 0;
  stbi_set_flip_vertically_on_load(0);
  stbi_uc* img = stbi_load_from_memory(data, (int)size, &w, &h, &ch, 3);
  if (!img || w <= 0 || h <= 0) return false;
  outWidth = w;
  outHeight = h;
  size_t n = (size_t)(w * h * 3);
  outRgb.resize(n);
  memcpy(outRgb.data(), img, n);
  stbi_image_free(img);
  return true;
}

#ifdef _WIN32
// MF H.264 decoder expects Annex B (start codes). Encoder often outputs AVCC (4-byte length + NAL).
static bool avccToAnnexB(const uint8_t* data, size_t size, std::vector<uint8_t>& out) {
  if (size < 4) return false;
  const unsigned char annexB4[] = { 0x00, 0x00, 0x00, 0x01 };

  // First, see if this is a well-formed AVCC sample (4-byte NAL lengths that consume the full buffer).
  size_t probe = 0;
  int nalCount = 0;
  while (probe + 4 <= size) {
    uint32_t len = (uint32_t)data[probe] << 24 | (uint32_t)data[probe + 1] << 16
                 | (uint32_t)data[probe + 2] << 8 | data[probe + 3];
    if (len == 0 || probe + 4 + len > size) {
      nalCount = 0;
      break;
    }
    probe += 4 + len;
    ++nalCount;
  }
  if (nalCount > 0 && probe == size) {
    out.clear();
    out.reserve(size + (size_t)nalCount * 4);
    size_t offset = 0;
    while (offset + 4 <= size) {
      uint32_t len = (uint32_t)data[offset] << 24 | (uint32_t)data[offset + 1] << 16
                   | (uint32_t)data[offset + 2] << 8 | data[offset + 3];
      out.insert(out.end(), annexB4, annexB4 + 4);
      out.insert(out.end(), data + offset + 4, data + offset + 4 + len);
      offset += 4 + len;
    }
    return true;
  }

  // Not valid AVCC; if it starts with Annex-B prefix, keep original.
  if (size >= 4 && data[0] == 0 && data[1] == 0 &&
      ((data[2] == 1) || (data[2] == 0 && data[3] == 1))) {
    return false;
  }

  // Unknown layout; keep original and let decoder handle/reject.
  return false;
}

static void nv12ToRgb(const uint8_t* yPlane, const uint8_t* uvPlane, int w, int h, int yStride,
                      std::vector<uint8_t>& outRgb) {
  outRgb.resize((size_t)w * h * 3);
  for (int j = 0; j < h; ++j) {
    for (int i = 0; i < w; ++i) {
      int y = yPlane[j * yStride + i];
      int u = uvPlane[(j / 2) * yStride + (i & ~1)] - 128;
      int v = uvPlane[(j / 2) * yStride + (i & ~1) + 1] - 128;
      int r = y + (351 * v) / 256;
      int g = y - (179 * v + 86 * u) / 256;
      int b = y + (443 * u) / 256;
      if (r < 0) r = 0; if (r > 255) r = 255;
      if (g < 0) g = 0; if (g > 255) g = 255;
      if (b < 0) b = 0; if (b > 255) b = 255;
      size_t idx = (size_t)(j * w + i) * 3;
      outRgb[idx + 0] = (uint8_t)r;
      outRgb[idx + 1] = (uint8_t)g;
      outRgb[idx + 2] = (uint8_t)b;
    }
  }
}
#endif

bool FrameDecoder::decodeH264(const uint8_t* data, size_t size,
                              std::vector<uint8_t>& outRgb, int& outWidth, int& outHeight,
                              bool* needMoreInput) {
  if (needMoreInput) *needMoreInput = false;
#ifdef _WIN32
  // Minimal MF H.264 decoder: create MFT, feed input, drain output (NV12), convert to RGB.
  MFDecoderState* s = (MFDecoderState*)mfDecoder_;
  if (!s) {
    s = new MFDecoderState();
    mfDecoder_ = s;
    static bool mfStarted = false;
    if (!mfStarted) {
      if (MFStartup(MF_VERSION, MFSTARTUP_NOSOCKET) != S_OK) {
        delete s;
        mfDecoder_ = nullptr;
        return false;
      }
      mfStarted = true;
    }
    IMFTransform* mft = nullptr;
    UINT32 count = 0;
    IMFActivate** ppActivate = nullptr;
    if (MFTEnumEx(MFT_CATEGORY_VIDEO_DECODER, MFT_ENUM_FLAG_SYNCMFT | MFT_ENUM_FLAG_LOCALMFT,
                  nullptr, nullptr, &ppActivate, &count) != S_OK || count == 0) {
      delete s;
      mfDecoder_ = nullptr;
      return false;
    }
    bool found = false;
    for (UINT32 i = 0; i < count; ++i) {
      if (ppActivate[i]->ActivateObject(IID_PPV_ARGS(&mft)) == S_OK && mft) {
        HRESULT hr = S_OK;
        IMFMediaType* inType = nullptr;
        hr = MFCreateMediaType(&inType);
        if (SUCCEEDED(hr)) hr = inType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
        if (SUCCEEDED(hr)) hr = inType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264);
        if (SUCCEEDED(hr)) hr = MFSetAttributeSize(inType, MF_MT_FRAME_SIZE, 960, 540);
        if (SUCCEEDED(hr)) hr = inType->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
        if (SUCCEEDED(hr)) hr = MFSetAttributeRatio(inType, MF_MT_FRAME_RATE, 90, 1);
        if (SUCCEEDED(hr)) hr = MFSetAttributeRatio(inType, MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
        if (SUCCEEDED(hr)) hr = mft->SetInputType(0, inType, 0);
        if (inType) inType->Release();
        if (FAILED(hr)) {
          mft->Release();
          mft = nullptr;
          continue;
        }
        IMFMediaType* outType = nullptr;
        hr = MFCreateMediaType(&outType);
        if (SUCCEEDED(hr)) hr = outType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
        if (SUCCEEDED(hr)) hr = outType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_NV12);
        if (SUCCEEDED(hr)) hr = MFSetAttributeSize(outType, MF_MT_FRAME_SIZE, 960, 540);
        if (SUCCEEDED(hr)) hr = outType->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
        if (SUCCEEDED(hr)) hr = mft->SetOutputType(0, outType, 0);
        if (outType) outType->Release();
        if (FAILED(hr)) {
          mft->Release();
          mft = nullptr;
          continue;
        }
        mft->ProcessMessage(MFT_MESSAGE_COMMAND_FLUSH, 0);
        mft->ProcessMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, 0);
        mft->ProcessMessage(MFT_MESSAGE_NOTIFY_START_OF_STREAM, 0);
        s->mft = mft;
        s->inputType = nullptr;
        s->outputType = nullptr;
        s->width = 960;
        s->height = 540;
        s->sampleTime = 0;
        s->sampleDuration = 111111;
        found = true;
        break;
      }
    }
    for (UINT32 i = 0; i < count; ++i)
      ppActivate[i]->Release();
    CoTaskMemFree(ppActivate);
    if (!found) {
      delete s;
      mfDecoder_ = nullptr;
      return false;
    }
  }

  const uint8_t* decodeData = data;
  size_t decodeSize = size;
  std::vector<uint8_t> annexB;
  if (avccToAnnexB(data, size, annexB)) {
    decodeData = annexB.data();
    decodeSize = annexB.size();
  }

  IMFSample* inSample = nullptr;
  IMFMediaBuffer* inBuf = nullptr;
  if (MFCreateMemoryBuffer((DWORD)decodeSize, &inBuf) != S_OK) return false;
  BYTE* pBuf = nullptr;
  DWORD cap = 0;
  if (inBuf->Lock(&pBuf, &cap, nullptr) != S_OK || !pBuf || cap < decodeSize) {
    inBuf->Release();
    return false;
  }
  memcpy(pBuf, decodeData, decodeSize);
  inBuf->Unlock();
  inBuf->SetCurrentLength((DWORD)decodeSize);
  if (MFCreateSample(&inSample) != S_OK) {
    inBuf->Release();
    return false;
  }
  inSample->AddBuffer(inBuf);
  inBuf->Release();
  inSample->SetSampleTime(s->sampleTime);
  inSample->SetSampleDuration(s->sampleDuration);
  s->sampleTime += s->sampleDuration;

  HRESULT inputHr = s->mft->ProcessInput(0, inSample, 0);
  if (inputHr != S_OK) {
    std::fprintf(stderr, "[WinClient MF] ProcessInput failed hr=0x%08lX size=%zu\n",
                 static_cast<unsigned long>(inputHr), decodeSize);
    inSample->Release();
    return false;
  }
  inSample->Release();

  MFT_OUTPUT_STREAM_INFO osi{};
  if (s->mft->GetOutputStreamInfo(0, &osi) != S_OK) return false;
  const bool mftProvidesSamples = (osi.dwFlags & MFT_OUTPUT_STREAM_PROVIDES_SAMPLES) != 0;
  const DWORD outBufSize = (osi.cbSize != 0) ? osi.cbSize : (960 * 540 * 3 / 2); // NV12 fallback

  IMFSample* outSample = nullptr;
  MFT_OUTPUT_DATA_BUFFER outBuf{};
  outBuf.dwStreamID = 0;
  IMFMediaBuffer* outMediaBufAlloc = nullptr;
  IMFSample* outSampleAlloc = nullptr;

  for (int drain = 0; drain < 8; ++drain) {
    if (!mftProvidesSamples && outBuf.pSample == nullptr) {
      if (MFCreateMemoryBuffer(outBufSize, &outMediaBufAlloc) != S_OK) break;
      if (MFCreateSample(&outSampleAlloc) != S_OK) {
        outMediaBufAlloc->Release();
        break;
      }
      outSampleAlloc->AddBuffer(outMediaBufAlloc);
      outMediaBufAlloc->Release();
      outBuf.pSample = outSampleAlloc;
    }
    HRESULT hr = s->mft->ProcessOutput(0, 1, &outBuf, &outBuf.dwStatus);
    if (hr == MF_E_TRANSFORM_STREAM_CHANGE) {
      IMFMediaType* newType = nullptr;
      if (s->mft->GetOutputCurrentType(0, &newType) == S_OK && newType) {
        s->mft->SetOutputType(0, newType, 0);
        newType->Release();
      }
      if (outBuf.pSample) { outBuf.pSample->Release(); outBuf.pSample = nullptr; }
      outSampleAlloc = nullptr;
      continue;
    }
    if (hr == MF_E_TRANSFORM_NEED_MORE_INPUT) {
      if (outBuf.pSample && outBuf.pSample != outSampleAlloc) outBuf.pSample->Release();
      if (outSampleAlloc) outSampleAlloc->Release();
      if (needMoreInput) *needMoreInput = true;
      return false;
    }
    if (FAILED(hr) || outBuf.pSample == nullptr) {
      if (FAILED(hr)) {
        std::fprintf(stderr, "[WinClient MF] ProcessOutput failed hr=0x%08lX status=0x%08lX\n",
                     static_cast<unsigned long>(hr), static_cast<unsigned long>(outBuf.dwStatus));
      }
      if (outBuf.pSample) outBuf.pSample->Release();
      return false;
    }
    break;
  }
  outSample = outBuf.pSample;
  if (!outSample) return false;
  outSampleAlloc = nullptr;

  IMFMediaBuffer* outMediaBuf = nullptr;
  if (outSample->ConvertToContiguousBuffer(&outMediaBuf) != S_OK) {
    outSample->Release();
    return false;
  }
  BYTE* pOut = nullptr;
  DWORD outLen = 0;
  DWORD maxLen = 0;
  if (outMediaBuf->Lock(&pOut, &maxLen, &outLen) != S_OK || !pOut) {
    outMediaBuf->Release();
    outSample->Release();
    return false;
  }
  if (!pOut || outLen == 0) {
    outMediaBuf->Unlock();
    outMediaBuf->Release();
    outSample->Release();
    return false;
  }

  // NV12: Y plane then UV interleaved. Server sends 960x540.
  UINT32 w = 960, h = 540;
  IMFMediaType* outType = nullptr;
  if (s->mft->GetOutputCurrentType(0, &outType) == S_OK && outType) {
    MFGetAttributeSize(outType, MF_MT_FRAME_SIZE, &w, &h);
    outType->Release();
  }
  if (w == 0) w = 960;
  if (h == 0) h = 540;
  int stride = (int)w;
  const uint8_t* yPlane = pOut;
  const uint8_t* uvPlane = pOut + (size_t)stride * (int)h;
  nv12ToRgb(yPlane, uvPlane, (int)w, (int)h, stride, outRgb);
  outWidth = (int)w;
  outHeight = (int)h;

  outMediaBuf->Unlock();
  outMediaBuf->Release();
  outSample->Release();
  return true;
#else
  (void)data;
  (void)size;
  (void)outRgb;
  (void)outWidth;
  (void)outHeight;
  return false;
#endif
}

bool FrameDecoder::decode(uint8_t format, const uint8_t* data, size_t size,
                          std::vector<uint8_t>& outRgb, int& outWidth, int& outHeight,
                          bool* needMoreInput) {
  if (!data || size == 0) return false;
  if (format == 0)
    return decodeJpeg(data, size, outRgb, outWidth, outHeight);
  if (format == 1)
    return decodeH264(data, size, outRgb, outWidth, outHeight, needMoreInput);
  return false;
}

} // namespace winclient
