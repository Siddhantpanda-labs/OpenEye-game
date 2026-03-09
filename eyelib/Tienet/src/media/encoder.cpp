#include "tienet/media/encoder.hpp"
#include <cstdio>
#ifdef _WIN32
#include <mfapi.h>
#include <mfidl.h>
#include <mftransform.h>
#include <mferror.h>
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "mf.lib")
#endif
#if defined(_WIN32) && defined(TIENET_HAS_NVENC)
#include <d3d11.h>
#pragma comment(lib, "d3d11.lib")
#include <windows.h>
#include <nvEncodeAPI.h>
#endif

namespace {
	/** NVENC expects pitch aligned (e.g. 256) to avoid internal repacks. */
	inline uint32_t AlignPitch(uint32_t width, uint32_t alignment = 256u) {
		return (width + alignment - 1u) & ~(alignment - 1u);
	}
	/** BT.709 (sRGB) coefficients; avoid BT.601 for PC rendering. */
	inline void RgbToYuv709(uint8_t r, uint8_t g, uint8_t b, int& Y, int& U, int& V) {
		Y = ((54 * (int)r + 182 * (int)g + 18 * (int)b) >> 8) + 16;
		U = ((-29 * (int)r - 98 * (int)g + 128 * (int)b) >> 8) + 128;
		V = ((128 * (int)r - 116 * (int)g - 12 * (int)b) >> 8) + 128;
		if (Y < 0) Y = 0; if (Y > 255) Y = 255;
		if (U < 0) U = 0; if (U > 255) U = 255;
		if (V < 0) V = 0; if (V > 255) V = 255;
	}
}

namespace tienet {
namespace media {

static inline void mfSafeRelease(IUnknown* p) { if (p) p->Release(); }

class MFEncoder final : public IEncoder {
public:
    bool initialize(const EncoderConfig& cfg) override {
        m_cfg = cfg;
#ifdef _WIN32
        std::fprintf(stderr, "[MF] encoder init started (%u x %u)\n", m_cfg.width, m_cfg.height);
        shutdown();
        if (!s_mfStarted) {
            HRESULT hr = MFStartup(MF_VERSION, MFSTARTUP_NOSOCKET);
            if (FAILED(hr)) {
                std::fprintf(stderr, "[MF] encoder init failed: MFStartup failed, hr=0x%08lX\n", static_cast<unsigned long>(hr));
                return false;
            }
            s_mfStarted = true;
        }
        IMFActivate** ppActivate = nullptr; UINT32 count = 0;
        HRESULT hr = MFTEnumEx(MFT_CATEGORY_VIDEO_ENCODER, MFT_ENUM_FLAG_SYNCMFT | MFT_ENUM_FLAG_LOCALMFT,
                               nullptr, nullptr, &ppActivate, &count);
        if (FAILED(hr)) {
            std::fprintf(stderr, "[MF] encoder init failed: MFTEnumEx failed, hr=0x%08lX\n", static_cast<unsigned long>(hr));
            if (ppActivate) CoTaskMemFree(ppActivate);
            return false;
        }
        if (count == 0) {
            std::fprintf(stderr, "[MF] encoder init failed: MFTEnumEx returned 0 video encoders\n");
            if (ppActivate) CoTaskMemFree(ppActivate);
            return false;
        }
        HRESULT lastHr = E_FAIL;
        for (UINT32 i = 0; i < count; ++i) {
            IMFTransform* candidateMft = nullptr;
            IMFMediaType* candidateOutType = nullptr;
            IMFMediaType* candidateInType = nullptr;

            hr = ppActivate[i]->ActivateObject(IID_PPV_ARGS(&candidateMft));
            if (FAILED(hr) || !candidateMft) {
                std::fprintf(stderr, "[MF] candidate %u: ActivateObject failed, hr=0x%08lX\n", i, static_cast<unsigned long>(hr));
                lastHr = hr;
                continue;
            }

            // Configure output first (industry-safe order for many MFTs).
            hr = MFCreateMediaType(&candidateOutType);
            if (FAILED(hr)) {
                std::fprintf(stderr, "[MF] candidate %u: MFCreateMediaType(out) failed, hr=0x%08lX\n", i, static_cast<unsigned long>(hr));
                lastHr = hr;
                mfSafeRelease(candidateMft);
                continue;
            }
            hr = candidateOutType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
            if (SUCCEEDED(hr)) hr = candidateOutType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264);
            if (SUCCEEDED(hr)) hr = MFSetAttributeSize(candidateOutType, MF_MT_FRAME_SIZE, m_cfg.width, m_cfg.height);
            if (SUCCEEDED(hr)) hr = MFSetAttributeRatio(candidateOutType, MF_MT_FRAME_RATE, m_cfg.fps, 1);
            if (SUCCEEDED(hr)) hr = MFSetAttributeRatio(candidateOutType, MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
            if (SUCCEEDED(hr)) hr = candidateOutType->SetUINT32(MF_MT_AVG_BITRATE, m_cfg.bitrateBps);
            if (SUCCEEDED(hr)) hr = candidateOutType->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
            if (FAILED(hr)) {
                std::fprintf(stderr, "[MF] candidate %u: configure output type attrs failed, hr=0x%08lX\n", i, static_cast<unsigned long>(hr));
                lastHr = hr;
                mfSafeRelease(candidateOutType);
                mfSafeRelease(candidateMft);
                continue;
            }
            hr = candidateMft->SetOutputType(0, candidateOutType, 0);
            if (FAILED(hr)) {
                std::fprintf(stderr, "[MF] candidate %u: SetOutputType(H264) failed, hr=0x%08lX\n", i, static_cast<unsigned long>(hr));
                lastHr = hr;
                mfSafeRelease(candidateOutType);
                mfSafeRelease(candidateMft);
                continue;
            }

            // Configure input as NV12 with explicit stride/interlace.
            hr = MFCreateMediaType(&candidateInType);
            if (FAILED(hr)) {
                std::fprintf(stderr, "[MF] candidate %u: MFCreateMediaType(in) failed, hr=0x%08lX\n", i, static_cast<unsigned long>(hr));
                lastHr = hr;
                mfSafeRelease(candidateOutType);
                mfSafeRelease(candidateMft);
                continue;
            }
            hr = candidateInType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
            if (SUCCEEDED(hr)) hr = candidateInType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_NV12);
            if (SUCCEEDED(hr)) hr = MFSetAttributeSize(candidateInType, MF_MT_FRAME_SIZE, m_cfg.width, m_cfg.height);
            if (SUCCEEDED(hr)) hr = MFSetAttributeRatio(candidateInType, MF_MT_FRAME_RATE, m_cfg.fps, 1);
            if (SUCCEEDED(hr)) hr = MFSetAttributeRatio(candidateInType, MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
            if (SUCCEEDED(hr)) hr = candidateInType->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
            if (SUCCEEDED(hr)) hr = candidateInType->SetUINT32(MF_MT_DEFAULT_STRIDE, m_cfg.width);
            if (FAILED(hr)) {
                std::fprintf(stderr, "[MF] candidate %u: configure input type attrs failed, hr=0x%08lX\n", i, static_cast<unsigned long>(hr));
                lastHr = hr;
                mfSafeRelease(candidateInType);
                mfSafeRelease(candidateOutType);
                mfSafeRelease(candidateMft);
                continue;
            }
            hr = candidateMft->SetInputType(0, candidateInType, 0);
            if (FAILED(hr)) {
                std::fprintf(stderr, "[MF] candidate %u: SetInputType(NV12) failed, hr=0x%08lX\n", i, static_cast<unsigned long>(hr));
                lastHr = hr;
                mfSafeRelease(candidateInType);
                mfSafeRelease(candidateOutType);
                mfSafeRelease(candidateMft);
                continue;
            }

            // Candidate accepted.
            m_mft = candidateMft;
            m_inType = candidateInType;
            m_outType = candidateOutType;
            std::fprintf(stderr, "[MF] candidate %u selected\n", i);
            break;
        }

        for (UINT32 i = 0; i < count; ++i) mfSafeRelease(ppActivate[i]);
        CoTaskMemFree(ppActivate);

        if (!m_mft || !m_inType || !m_outType) {
            std::fprintf(stderr, "[MF] encoder init failed: no compatible encoder candidate (last hr=0x%08lX)\n", static_cast<unsigned long>(lastHr));
            shutdown();
            return false;
        }

        m_mft->ProcessMessage(MFT_MESSAGE_COMMAND_FLUSH, 0);
        m_mft->ProcessMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, 0);
        m_mft->ProcessMessage(MFT_MESSAGE_NOTIFY_START_OF_STREAM, 0);
        m_initialized = true;
        m_sampleTime = 0;
        m_firstOutputLogged = false;
        std::fprintf(stderr, "[MF] encoder init OK\n");
        return true;
#else
        m_initialized = true; return true;
#endif
    }

    std::vector<uint8_t> encode(const std::vector<uint8_t>& rgbaFrame) override {
        if (!m_initialized) return {};
#ifdef _WIN32
        if (rgbaFrame.empty() || m_cfg.width == 0 || m_cfg.height == 0) return {};
        const uint32_t w = m_cfg.width, h = m_cfg.height;
        std::vector<uint8_t> nv12; nv12.resize(w * h + (w * h) / 2);
        const uint8_t* src = rgbaFrame.data();
        uint8_t* yPlane = nv12.data();
        uint8_t* uvPlane = nv12.data() + w * h;
        for (uint32_t j = 0; j < h; ++j) {
            for (uint32_t i = 0; i < w; ++i) {
                size_t idx = (static_cast<size_t>(j) * w + i) * 4;
                uint8_t r = src[idx + 0], g = src[idx + 1], b = src[idx + 2];
                int Y = (66 * r + 129 * g + 25 * b + 128) >> 8; Y += 16; if (Y < 0) Y = 0; if (Y > 255) Y = 255;
                yPlane[j * w + i] = static_cast<uint8_t>(Y);
            }
        }
        for (uint32_t j = 0; j < h; j += 2) {
            for (uint32_t i = 0; i < w; i += 2) {
                size_t idx00 = (static_cast<size_t>(j) * w + i) * 4;
                size_t idx01 = (static_cast<size_t>(j) * w + i + 1) * 4;
                size_t idx10 = (static_cast<size_t>(j + 1) * w + i) * 4;
                size_t idx11 = (static_cast<size_t>(j + 1) * w + i + 1) * 4;
                auto samp = [&](size_t idx){ uint8_t r=src[idx], g=src[idx+1], b=src[idx+2];
                    int U = ((-38 * r - 74 * g + 112 * b + 128) >> 8) + 128;
                    int V = ((112 * r - 94 * g - 18 * b + 128) >> 8) + 128;
                    if (U<0)U=0; if(U>255)U=255; if(V<0)V=0; if(V>255)V=255; return std::pair<int,int>(U,V); };
                auto u0v0=samp(idx00), u0v1=samp(idx01), u1v0=samp(idx10), u1v1=samp(idx11);
                int U=(u0v0.first+u0v1.first+u1v0.first+u1v1.first)/4;
                int V=(u0v0.second+u0v1.second+u1v0.second+u1v1.second)/4;
                size_t uvIndex = (j/2) * w + i;
                uvPlane[uvIndex + 0] = static_cast<uint8_t>(U);
                uvPlane[uvIndex + 1] = static_cast<uint8_t>(V);
            }
        }
        return encodeNV12FrameMF(nv12.data(), nv12.size());
#else
        return rgbaFrame;
#endif
    }

    void shutdown() override {
        m_initialized = false;
#ifdef _WIN32
        mfSafeRelease(m_outType);
        mfSafeRelease(m_inType);
        mfSafeRelease(m_mft);
#endif
    }

    bool reconfigure(const EncoderConfig& cfg) override {
        if (!m_initialized) return false; m_cfg = cfg; return true;
    }

    void forceKeyframe() override { /* TODO: set encoder control for IDR */ }

    std::vector<uint8_t> encodeFromNV12(const std::vector<uint8_t>& nv12Frame, uint32_t pitchIn = 0) override {
        if (!m_initialized || nv12Frame.empty()) return {};
#ifdef _WIN32
        const uint32_t w = m_cfg.width, h = m_cfg.height;
        if (w == 0 || h == 0) return {};
        const uint32_t pitch = (pitchIn != 0) ? pitchIn : w;
        const size_t nv12Size = static_cast<size_t>(pitch) * h + static_cast<size_t>(pitch) * (h / 2);
        if (nv12Frame.size() < nv12Size) return {};

        const uint8_t* src = nv12Frame.data();
        const size_t contigSize = static_cast<size_t>(w) * h + (static_cast<size_t>(w) * h) / 2;
        std::vector<uint8_t> contiguous;
        const uint8_t* feedPtr = src;
        if (pitch != w) {
            contiguous.resize(contigSize);
            uint8_t* yDst = contiguous.data();
            uint8_t* uvDst = contiguous.data() + static_cast<size_t>(w) * h;
            for (uint32_t j = 0; j < h; ++j)
                memcpy(yDst + j * w, src + j * pitch, w);
            for (uint32_t j = 0; j < h / 2; ++j)
                memcpy(uvDst + j * w, src + static_cast<size_t>(pitch) * h + j * pitch, w);
            feedPtr = contiguous.data();
        }
        return encodeNV12FrameMF(feedPtr, contigSize);
#else
        (void)nv12Frame;
        (void)pitchIn;
        return {};
#endif
    }

private:
    EncoderConfig m_cfg{};
    bool m_initialized = false;
#ifdef _WIN32
    std::vector<uint8_t> encodeNV12FrameMF(const uint8_t* nv12Data, size_t cbBytes) {
        if (!m_mft || !nv12Data || cbBytes == 0) return {};
        if (cbBytes > static_cast<size_t>(0xFFFFFFFFu)) {
            std::fprintf(stderr, "[MF] encode failed: NV12 buffer too large (%zu)\n", cbBytes);
            return {};
        }

        // --- Build input sample ---
        IMFSample* sample = nullptr;
        IMFMediaBuffer* buffer = nullptr;
        HRESULT hr = MFCreateMemoryBuffer(static_cast<DWORD>(cbBytes), &buffer);
        if (FAILED(hr)) {
            std::fprintf(stderr, "[MF] encode failed: MFCreateMemoryBuffer(input), hr=0x%08lX\n", static_cast<unsigned long>(hr));
            return {};
        }
        BYTE* dst = nullptr;
        DWORD maxLen = 0, curLen = 0;
        hr = buffer->Lock(&dst, &maxLen, &curLen);
        if (FAILED(hr) || !dst) {
            std::fprintf(stderr, "[MF] encode failed: input buffer lock, hr=0x%08lX\n", static_cast<unsigned long>(hr));
            mfSafeRelease(buffer);
            return {};
        }
        memcpy(dst, nv12Data, static_cast<size_t>(cbBytes));
        buffer->Unlock();
        hr = buffer->SetCurrentLength(static_cast<DWORD>(cbBytes));
        if (FAILED(hr)) {
            std::fprintf(stderr, "[MF] encode failed: SetCurrentLength(input), hr=0x%08lX\n", static_cast<unsigned long>(hr));
            mfSafeRelease(buffer);
            return {};
        }

        hr = MFCreateSample(&sample);
        if (FAILED(hr)) {
            std::fprintf(stderr, "[MF] encode failed: MFCreateSample(input), hr=0x%08lX\n", static_cast<unsigned long>(hr));
            mfSafeRelease(buffer);
            return {};
        }
        hr = sample->AddBuffer(buffer);
        mfSafeRelease(buffer);
        if (FAILED(hr)) {
            std::fprintf(stderr, "[MF] encode failed: AddBuffer(input sample), hr=0x%08lX\n", static_cast<unsigned long>(hr));
            mfSafeRelease(sample);
            return {};
        }

        const LONGLONG frameDur = 10'000'000LL / (m_cfg.fps ? m_cfg.fps : 60);
        sample->SetSampleTime(m_sampleTime);
        sample->SetSampleDuration(frameDur);
        m_sampleTime += frameDur;

        // --- Feed input ---
        hr = m_mft->ProcessInput(0, sample, 0);
        mfSafeRelease(sample);
        if (FAILED(hr)) {
            std::fprintf(stderr, "[MF] encode failed: ProcessInput, hr=0x%08lX\n", static_cast<unsigned long>(hr));
            return {};
        }

        // --- Drain all available output (loop until NEED_MORE_INPUT) ---
        return drainMFTOutput(cbBytes);
    }

    std::vector<uint8_t> drainMFTOutput(size_t inputCbHint) {
        std::vector<uint8_t> bitstream;

        MFT_OUTPUT_STREAM_INFO osi{};
        HRESULT hr = m_mft->GetOutputStreamInfo(0, &osi);
        if (FAILED(hr)) {
            std::fprintf(stderr, "[MF] encode failed: GetOutputStreamInfo, hr=0x%08lX\n", static_cast<unsigned long>(hr));
            return {};
        }
        const bool mftProvidesSamples = (osi.dwFlags & MFT_OUTPUT_STREAM_PROVIDES_SAMPLES) != 0;
        const DWORD outCb = (osi.cbSize != 0) ? osi.cbSize : static_cast<DWORD>(inputCbHint);

        for (int drainIter = 0; drainIter < 32; ++drainIter) {
            IMFSample* outSample = nullptr;
            IMFMediaBuffer* outBuf = nullptr;
            MFT_OUTPUT_DATA_BUFFER odb{};
            odb.dwStreamID = 0;

            if (!mftProvidesSamples) {
                hr = MFCreateMemoryBuffer(outCb, &outBuf);
                if (FAILED(hr)) {
                    std::fprintf(stderr, "[MF] encode failed: MFCreateMemoryBuffer(output), hr=0x%08lX\n", static_cast<unsigned long>(hr));
                    break;
                }
                hr = MFCreateSample(&outSample);
                if (FAILED(hr)) {
                    std::fprintf(stderr, "[MF] encode failed: MFCreateSample(output), hr=0x%08lX\n", static_cast<unsigned long>(hr));
                    mfSafeRelease(outBuf);
                    break;
                }
                outSample->AddBuffer(outBuf);
                odb.pSample = outSample;
            }

            DWORD status = 0;
            hr = m_mft->ProcessOutput(0, 1, &odb, &status);

            if (hr == MF_E_TRANSFORM_NEED_MORE_INPUT) {
                mfSafeRelease(outBuf);
                mfSafeRelease(outSample);
                if (odb.pEvents) mfSafeRelease(odb.pEvents);
                break;
            }
            if (hr == MF_E_TRANSFORM_STREAM_CHANGE) {
                std::fprintf(stderr, "[MF] ProcessOutput stream change; reapplying output type\n");
                m_mft->SetOutputType(0, m_outType, 0);
                mfSafeRelease(outBuf);
                mfSafeRelease(outSample);
                if (odb.pEvents) mfSafeRelease(odb.pEvents);
                continue;
            }
            if (FAILED(hr)) {
                std::fprintf(stderr, "[MF] encode failed: ProcessOutput, hr=0x%08lX, status=0x%08lX\n",
                             static_cast<unsigned long>(hr), static_cast<unsigned long>(status));
                mfSafeRelease(outBuf);
                mfSafeRelease(outSample);
                if (odb.pEvents) mfSafeRelease(odb.pEvents);
                break;
            }

            // Extract encoded data from the produced sample.
            IMFSample* producedSample = odb.pSample ? odb.pSample : outSample;
            if (producedSample) {
                IMFMediaBuffer* contiguous = nullptr;
                hr = producedSample->ConvertToContiguousBuffer(&contiguous);
                if (SUCCEEDED(hr) && contiguous) {
                    BYTE* p = nullptr;
                    DWORD len = 0;
                    hr = contiguous->Lock(&p, nullptr, &len);
                    if (SUCCEEDED(hr) && p && len > 0) {
                        bitstream.insert(bitstream.end(), p, p + len);
                    }
                    contiguous->Unlock();
                    mfSafeRelease(contiguous);
                }
            }

            if (odb.pSample && odb.pSample != outSample) mfSafeRelease(odb.pSample);
            mfSafeRelease(outBuf);
            mfSafeRelease(outSample);
            if (odb.pEvents) mfSafeRelease(odb.pEvents);
        }

        if (!bitstream.empty() && !m_firstOutputLogged) {
            std::fprintf(stderr, "[MF] first H.264 output: %zu bytes\n", bitstream.size());
            m_firstOutputLogged = true;
        }
        return bitstream;
    }

    static inline bool s_mfStarted = false;
    IMFTransform* m_mft = nullptr; IMFMediaType* m_inType = nullptr; IMFMediaType* m_outType = nullptr;
    LONGLONG m_sampleTime = 0;
    bool m_firstOutputLogged = false;
#endif
};

class NVENCEncoder final : public IEncoder {
public:
	static constexpr int kBitstreamBufferCount = 3;

	bool initialize(const EncoderConfig& cfg) override {
		m_cfg = cfg;
#if defined(_WIN32) && defined(TIENET_HAS_NVENC)
		m_initialized = false;
		std::fprintf(stderr, "[NVENC] encoder init started (%u x %u)\n", m_cfg.width, m_cfg.height);
		if (!createD3D()) { std::fprintf(stderr, "[NVENC] encoder init failed: D3D11 device creation failed\n"); return false; }
		if (!loadNvenc()) { std::fprintf(stderr, "[NVENC] encoder init failed: load nvEncodeAPI64.dll failed\n"); return false; }
		if (!openSession()) { std::fprintf(stderr, "[NVENC] encoder init failed: open encode session failed\n"); return false; }
		if (!initEncoder()) { std::fprintf(stderr, "[NVENC] encoder init failed: initialize encoder failed\n"); return false; }
		for (int i = 0; i < kBitstreamBufferCount; ++i)
			if (!createBitstreamBuffer(i)) { std::fprintf(stderr, "[NVENC] encoder init failed: create bitstream buffer %d failed\n", i); return false; }
		m_curBitstreamIdx = 0;
		m_initialized = true;
		std::fprintf(stderr, "[NVENC] encoder init OK\n");
		return true;
#else
		m_initialized = true; // Fallback pass-through
		return true;
#endif
	}

	std::vector<uint8_t> encode(const std::vector<uint8_t>& rgbaFrame) override {
		if (!m_initialized) return {};
#if defined(_WIN32) && defined(TIENET_HAS_NVENC)
		if (!m_session) return rgbaFrame;
		if (rgbaFrame.empty() || m_cfg.width == 0 || m_cfg.height == 0) return {};
		const uint32_t w = m_cfg.width, h = m_cfg.height;
		const uint32_t pitch = AlignPitch(w);
		const size_t nv12Size = static_cast<size_t>(pitch) * h + static_cast<size_t>(pitch) * (h / 2);
		std::vector<uint8_t> nv12; nv12.resize(nv12Size);
		const uint8_t* src = rgbaFrame.data();
		uint8_t* yPlane = nv12.data();
		uint8_t* uvPlane = nv12.data() + static_cast<size_t>(pitch) * h;
		// BT.709 (sRGB); avoid BT.601 to prevent color shifts on PC.
		for (uint32_t j = 0; j < h; ++j) {
			for (uint32_t i = 0; i < w; ++i) {
				size_t idx = (static_cast<size_t>(j) * w + i) * 4;
				int Y, U, V;
				RgbToYuv709(src[idx + 0], src[idx + 1], src[idx + 2], Y, U, V);
				yPlane[j * pitch + i] = static_cast<uint8_t>(Y);
			}
		}
		for (uint32_t j = 0; j < h; j += 2) {
			for (uint32_t i = 0; i < w; i += 2) {
				size_t idx00 = (static_cast<size_t>(j) * w + i) * 4;
				size_t idx01 = (static_cast<size_t>(j) * w + i + 1) * 4;
				size_t idx10 = (static_cast<size_t>(j + 1) * w + i) * 4;
				size_t idx11 = (static_cast<size_t>(j + 1) * w + i + 1) * 4;
				int Y0, U0, V0; RgbToYuv709(src[idx00], src[idx00+1], src[idx00+2], Y0, U0, V0);
				int Y1, U1, V1; RgbToYuv709(src[idx01], src[idx01+1], src[idx01+2], Y1, U1, V1);
				int Y2, U2, V2; RgbToYuv709(src[idx10], src[idx10+1], src[idx10+2], Y2, U2, V2);
				int Y3, U3, V3; RgbToYuv709(src[idx11], src[idx11+1], src[idx11+2], Y3, U3, V3);
				int U = (U0 + U1 + U2 + U3) / 4, V = (V0 + V1 + V2 + V3) / 4;
				size_t uvOff = (j / 2) * pitch + i * 2;
				uvPlane[uvOff + 0] = static_cast<uint8_t>(U);
				uvPlane[uvOff + 1] = static_cast<uint8_t>(V);
			}
		}

		if (!m_inputTex && !createInputTexture()) { std::fprintf(stderr, "[NVENC] encode failed: createInputTexture failed\n"); return rgbaFrame; }
		D3D11_BOX box{}; box.left=0; box.top=0; box.front=0; box.back=1; box.right=w; box.bottom=h;
		m_ctx->UpdateSubresource(m_inputTex, 0, &box, nv12.data(), pitch, 0);

		NV_ENC_REGISTER_RESOURCE reg{}; reg.version = NV_ENC_REGISTER_RESOURCE_VER;
		reg.resourceType = NV_ENC_INPUT_RESOURCE_TYPE_DIRECTX; reg.resourceToRegister = m_inputTex;
		reg.width = w; reg.height = h; reg.pitch = pitch; reg.bufferFormat = NV_ENC_BUFFER_FORMAT_NV12;
		if (m_fn.nvEncRegisterResource(m_session, &reg) != NV_ENC_SUCCESS) { std::fprintf(stderr, "[NVENC] encode failed: nvEncRegisterResource failed\n"); return rgbaFrame; }
		NV_ENC_MAP_INPUT_RESOURCE map{}; map.version = NV_ENC_MAP_INPUT_RESOURCE_VER; map.registeredResource = reg.registeredResource;
		if (m_fn.nvEncMapInputResource(m_session, &map) != NV_ENC_SUCCESS) { m_fn.nvEncUnregisterResource(m_session, reg.registeredResource); std::fprintf(stderr, "[NVENC] encode failed: nvEncMapInputResource failed\n"); return rgbaFrame; }
		NV_ENC_PIC_PARAMS pic{}; pic.version = NV_ENC_PIC_PARAMS_VER; pic.inputBuffer = map.mappedResource;
		NV_ENC_OUTPUT_PTR bitstream = m_bitstreams[m_curBitstreamIdx];
		pic.bufferFmt = NV_ENC_BUFFER_FORMAT_NV12; pic.inputWidth = w; pic.inputHeight = h; pic.outputBitstream = bitstream; pic.pictureStruct = NV_ENC_PIC_STRUCT_FRAME;
		if (m_fn.nvEncEncodePicture(m_session, &pic) != NV_ENC_SUCCESS) {
			m_fn.nvEncUnmapInputResource(m_session, map.mappedResource);
			m_fn.nvEncUnregisterResource(m_session, reg.registeredResource);
			std::fprintf(stderr, "[NVENC] encode failed: nvEncEncodePicture failed\n");
			return rgbaFrame;
		}
		m_curBitstreamIdx = (m_curBitstreamIdx + 1) % kBitstreamBufferCount;
		NV_ENC_LOCK_BITSTREAM lock{}; lock.version = NV_ENC_LOCK_BITSTREAM_VER; lock.outputBitstream = bitstream; lock.doNotWait = 1;
		NVENCSTATUS st = m_fn.nvEncLockBitstream(m_session, &lock);
		if (st != NV_ENC_SUCCESS) {
			lock.doNotWait = 0;
			st = m_fn.nvEncLockBitstream(m_session, &lock);
		}
		if (st != NV_ENC_SUCCESS) {
			m_fn.nvEncUnmapInputResource(m_session, map.mappedResource);
			m_fn.nvEncUnregisterResource(m_session, reg.registeredResource);
			std::fprintf(stderr, "[NVENC] encode failed: nvEncLockBitstream failed (status %d)\n", static_cast<int>(st));
			return rgbaFrame;
		}
		std::vector<uint8_t> out;
		out.assign(static_cast<uint8_t*>(lock.bitstreamBufferPtr), static_cast<uint8_t*>(lock.bitstreamBufferPtr) + lock.bitstreamSizeInBytes);
		m_fn.nvEncUnlockBitstream(m_session, bitstream);
		m_fn.nvEncUnmapInputResource(m_session, map.mappedResource);
		m_fn.nvEncUnregisterResource(m_session, reg.registeredResource);
		return out;
#else
		return rgbaFrame;
#endif
	}

	std::vector<uint8_t> encodeFromNV12(const std::vector<uint8_t>& nv12Frame, uint32_t pitchIn = 0) override {
#if defined(_WIN32) && defined(TIENET_HAS_NVENC)
		if (!m_initialized || !m_session || nv12Frame.empty()) return {};
		const uint32_t w = m_cfg.width, h = m_cfg.height;
		const uint32_t pitch = pitchIn != 0 ? pitchIn : w;
		const size_t nv12Size = static_cast<size_t>(pitch) * h + static_cast<size_t>(pitch) * (h / 2);
		if (nv12Frame.size() < nv12Size) return {};
		if (!m_inputTex && !createInputTexture()) { std::fprintf(stderr, "[NVENC] encodeFromNV12 failed: createInputTexture failed\n"); return {}; }
		D3D11_BOX box{}; box.left = 0; box.top = 0; box.front = 0; box.back = 1; box.right = w; box.bottom = h;
		m_ctx->UpdateSubresource(m_inputTex, 0, &box, nv12Frame.data(), pitch, 0);
		NV_ENC_REGISTER_RESOURCE reg{}; reg.version = NV_ENC_REGISTER_RESOURCE_VER;
		reg.resourceType = NV_ENC_INPUT_RESOURCE_TYPE_DIRECTX; reg.resourceToRegister = m_inputTex;
		reg.width = w; reg.height = h; reg.pitch = pitch; reg.bufferFormat = NV_ENC_BUFFER_FORMAT_NV12;
		if (m_fn.nvEncRegisterResource(m_session, &reg) != NV_ENC_SUCCESS) { std::fprintf(stderr, "[NVENC] encodeFromNV12 failed: nvEncRegisterResource failed\n"); return {}; }
		NV_ENC_MAP_INPUT_RESOURCE map{}; map.version = NV_ENC_MAP_INPUT_RESOURCE_VER; map.registeredResource = reg.registeredResource;
		if (m_fn.nvEncMapInputResource(m_session, &map) != NV_ENC_SUCCESS) { m_fn.nvEncUnregisterResource(m_session, reg.registeredResource); std::fprintf(stderr, "[NVENC] encodeFromNV12 failed: nvEncMapInputResource failed\n"); return {}; }
		NV_ENC_PIC_PARAMS pic{}; pic.version = NV_ENC_PIC_PARAMS_VER; pic.inputBuffer = map.mappedResource;
		NV_ENC_OUTPUT_PTR bitstream = m_bitstreams[m_curBitstreamIdx];
		pic.bufferFmt = NV_ENC_BUFFER_FORMAT_NV12; pic.inputWidth = w; pic.inputHeight = h; pic.outputBitstream = bitstream; pic.pictureStruct = NV_ENC_PIC_STRUCT_FRAME;
		if (m_fn.nvEncEncodePicture(m_session, &pic) != NV_ENC_SUCCESS) {
			m_fn.nvEncUnmapInputResource(m_session, map.mappedResource);
			m_fn.nvEncUnregisterResource(m_session, reg.registeredResource);
			std::fprintf(stderr, "[NVENC] encodeFromNV12 failed: nvEncEncodePicture failed\n");
			return {};
		}
		m_curBitstreamIdx = (m_curBitstreamIdx + 1) % kBitstreamBufferCount;
		NV_ENC_LOCK_BITSTREAM lock{}; lock.version = NV_ENC_LOCK_BITSTREAM_VER; lock.outputBitstream = bitstream; lock.doNotWait = 1;
		NVENCSTATUS st = m_fn.nvEncLockBitstream(m_session, &lock);
		if (st != NV_ENC_SUCCESS) { lock.doNotWait = 0; st = m_fn.nvEncLockBitstream(m_session, &lock); }
		if (st != NV_ENC_SUCCESS) {
			m_fn.nvEncUnmapInputResource(m_session, map.mappedResource);
			m_fn.nvEncUnregisterResource(m_session, reg.registeredResource);
			std::fprintf(stderr, "[NVENC] encodeFromNV12 failed: nvEncLockBitstream failed (status %d)\n", static_cast<int>(st));
			return {};
		}
		std::vector<uint8_t> out(static_cast<uint8_t*>(lock.bitstreamBufferPtr), static_cast<uint8_t*>(lock.bitstreamBufferPtr) + lock.bitstreamSizeInBytes);
		m_fn.nvEncUnlockBitstream(m_session, bitstream);
		m_fn.nvEncUnmapInputResource(m_session, map.mappedResource);
		m_fn.nvEncUnregisterResource(m_session, reg.registeredResource);
		return out;
#else
		(void)nv12Frame;
		return {};
#endif
	}

	std::vector<uint8_t> encodeFromD3D11NV12(void* d3d11TextureNV12) override {
#if defined(_WIN32) && defined(TIENET_HAS_NVENC)
		return encodeFromD3D11NV12Impl(d3d11TextureNV12);
#else
		(void)d3d11TextureNV12;
		return {};
#endif
	}

#if defined(_WIN32) && defined(TIENET_HAS_NVENC)
	std::vector<uint8_t> encodeFromD3D11NV12Impl(void* d3d11TextureNV12) {
		if (!m_initialized || !m_session || !d3d11TextureNV12) return {};
		ID3D11Texture2D* tex = static_cast<ID3D11Texture2D*>(d3d11TextureNV12);
		const uint32_t w = m_cfg.width, h = m_cfg.height;
		const uint32_t pitch = AlignPitch(w);
		NV_ENC_REGISTER_RESOURCE reg{}; reg.version = NV_ENC_REGISTER_RESOURCE_VER;
		reg.resourceType = NV_ENC_INPUT_RESOURCE_TYPE_DIRECTX; reg.resourceToRegister = tex;
		reg.width = w; reg.height = h; reg.pitch = pitch; reg.bufferFormat = NV_ENC_BUFFER_FORMAT_NV12;
		if (m_fn.nvEncRegisterResource(m_session, &reg) != NV_ENC_SUCCESS) return {};
		NV_ENC_MAP_INPUT_RESOURCE map{}; map.version = NV_ENC_MAP_INPUT_RESOURCE_VER; map.registeredResource = reg.registeredResource;
		if (m_fn.nvEncMapInputResource(m_session, &map) != NV_ENC_SUCCESS) { m_fn.nvEncUnregisterResource(m_session, reg.registeredResource); return {}; }
		NV_ENC_PIC_PARAMS pic{}; pic.version = NV_ENC_PIC_PARAMS_VER; pic.inputBuffer = map.mappedResource;
		NV_ENC_OUTPUT_PTR bitstream = m_bitstreams[m_curBitstreamIdx];
		pic.bufferFmt = NV_ENC_BUFFER_FORMAT_NV12; pic.inputWidth = w; pic.inputHeight = h; pic.outputBitstream = bitstream; pic.pictureStruct = NV_ENC_PIC_STRUCT_FRAME;
		if (m_fn.nvEncEncodePicture(m_session, &pic) != NV_ENC_SUCCESS) {
			m_fn.nvEncUnmapInputResource(m_session, map.mappedResource);
			m_fn.nvEncUnregisterResource(m_session, reg.registeredResource);
			return {};
		}
		m_curBitstreamIdx = (m_curBitstreamIdx + 1) % kBitstreamBufferCount;
		NV_ENC_LOCK_BITSTREAM lock{}; lock.version = NV_ENC_LOCK_BITSTREAM_VER; lock.outputBitstream = bitstream; lock.doNotWait = 1;
		NVENCSTATUS st = m_fn.nvEncLockBitstream(m_session, &lock);
		if (st != NV_ENC_SUCCESS) { lock.doNotWait = 0; st = m_fn.nvEncLockBitstream(m_session, &lock); }
		if (st != NV_ENC_SUCCESS) {
			m_fn.nvEncUnmapInputResource(m_session, map.mappedResource);
			m_fn.nvEncUnregisterResource(m_session, reg.registeredResource);
			return {};
		}
		std::vector<uint8_t> out(static_cast<uint8_t*>(lock.bitstreamBufferPtr), static_cast<uint8_t*>(lock.bitstreamBufferPtr) + lock.bitstreamSizeInBytes);
		m_fn.nvEncUnlockBitstream(m_session, bitstream);
		m_fn.nvEncUnmapInputResource(m_session, map.mappedResource);
		m_fn.nvEncUnregisterResource(m_session, reg.registeredResource);
		return out;
	}
#endif

	void shutdown() override { m_initialized = false; }

    bool reconfigure(const EncoderConfig& cfg) override {
        if (!m_initialized) return false;
        m_cfg = cfg; return true;
    }

    void forceKeyframe() override { /* placeholder */ }

#if defined(TIENET_HAS_CUDA)
    std::vector<uint8_t> encodeCudaNV12(void* cudaDevicePtrY, void* cudaDevicePtrUV, uint32_t pitch) override {
#if defined(_WIN32) && defined(TIENET_HAS_NVENC)
        if (!m_initialized || !m_session) return {};
        // Register CUDA device pointers and encode (simplified; real impl uses NV_ENC_INPUT_RESOURCE_TYPE_CUDADEVICEPTR)
        NV_ENC_REGISTER_RESOURCE reg{}; reg.version = NV_ENC_REGISTER_RESOURCE_VER;
        reg.resourceType = NV_ENC_INPUT_RESOURCE_TYPE_CUDADEVICEPTR;
        reg.resourceToRegister = cudaDevicePtrY;
        reg.width = m_cfg.width; reg.height = m_cfg.height; reg.pitch = pitch; reg.bufferFormat = NV_ENC_BUFFER_FORMAT_NV12;
        if (m_fn.nvEncRegisterResource(m_session, &reg) != NV_ENC_SUCCESS) return {};
        NV_ENC_MAP_INPUT_RESOURCE map{}; map.version = NV_ENC_MAP_INPUT_RESOURCE_VER; map.registeredResource = reg.registeredResource;
        if (m_fn.nvEncMapInputResource(m_session, &map) != NV_ENC_SUCCESS) { m_fn.nvEncUnregisterResource(m_session, reg.registeredResource); return {}; }
        NV_ENC_PIC_PARAMS pic{}; pic.version = NV_ENC_PIC_PARAMS_VER; pic.inputBuffer = map.mappedResource; pic.bufferFmt = NV_ENC_BUFFER_FORMAT_NV12;
        pic.inputWidth = m_cfg.width; pic.inputHeight = m_cfg.height; pic.outputBitstream = m_bitstreams[0]; pic.pictureStruct = NV_ENC_PIC_STRUCT_FRAME;
        if (m_fn.nvEncEncodePicture(m_session, &pic) != NV_ENC_SUCCESS) {
            m_fn.nvEncUnmapInputResource(m_session, map.mappedResource);
            m_fn.nvEncUnregisterResource(m_session, reg.registeredResource);
            return {};
        }
        NV_ENC_LOCK_BITSTREAM lock{}; lock.version = NV_ENC_LOCK_BITSTREAM_VER; lock.outputBitstream = m_bitstreams[0]; lock.doNotWait = 1;
        if (m_fn.nvEncLockBitstream(m_session, &lock) != NV_ENC_SUCCESS) {
            lock.doNotWait = 0;
            if (m_fn.nvEncLockBitstream(m_session, &lock) != NV_ENC_SUCCESS) {
                m_fn.nvEncUnmapInputResource(m_session, map.mappedResource);
                m_fn.nvEncUnregisterResource(m_session, reg.registeredResource);
                return {};
            }
        }
        std::vector<uint8_t> out;
        out.assign(static_cast<uint8_t*>(lock.bitstreamBufferPtr), static_cast<uint8_t*>(lock.bitstreamBufferPtr) + lock.bitstreamSizeInBytes);
        m_fn.nvEncUnlockBitstream(m_session, m_bitstreams[0]);
        m_fn.nvEncUnmapInputResource(m_session, map.mappedResource);
        m_fn.nvEncUnregisterResource(m_session, reg.registeredResource);
        return out;
#else
        return {};
#endif
    }
#endif

private:
	EncoderConfig m_cfg{};
	bool m_initialized = false;
#if defined(_WIN32) && defined(TIENET_HAS_NVENC)
	// D3D11
	ID3D11Device* m_d3d = nullptr; ID3D11DeviceContext* m_ctx = nullptr; ID3D11Texture2D* m_inputTex = nullptr;
	// NVENC
	HMODULE m_nvLib = nullptr; NV_ENCODE_API_FUNCTION_LIST m_fn{}; void* m_session = nullptr;
	NV_ENC_OUTPUT_PTR m_bitstreams[kBitstreamBufferCount] = {};
	int m_curBitstreamIdx = 0;
	NV_ENC_INITIALIZE_PARAMS m_init{}; NV_ENC_CONFIG m_encConfig{};

	bool createD3D() {
		UINT flags = 0; D3D_FEATURE_LEVEL fl;
		HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags, nullptr, 0, D3D11_SDK_VERSION, &m_d3d, &fl, &m_ctx);
		return SUCCEEDED(hr);
	}
	bool loadNvenc() {
		m_nvLib = LoadLibraryA("nvEncodeAPI64.dll");
		if (!m_nvLib) return false;
		auto createFn = (NVENCSTATUS (NVENCAPI*)(NV_ENCODE_API_FUNCTION_LIST*))GetProcAddress(m_nvLib, "NvEncodeAPICreateInstance");
		if (!createFn) return false;
		memset(&m_fn, 0, sizeof(m_fn)); m_fn.version = NV_ENCODE_API_FUNCTION_LIST_VER;
		return createFn(&m_fn) == NV_ENC_SUCCESS;
	}
	bool openSession() {
		NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS p{}; p.version = NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS_VER;
		p.device = m_d3d; p.deviceType = NV_ENC_DEVICE_TYPE_DIRECTX;
		return m_fn.nvEncOpenEncodeSessionEx(&p, &m_session) == NV_ENC_SUCCESS;
	}
	bool initEncoder() {
		GUID codec = NV_ENC_CODEC_H264_GUID; 
		GUID preset;
#if defined(NV_ENC_PRESET_P3_GUID)
		preset = NV_ENC_PRESET_P3_GUID; // balanced low-latency friendly
#elif defined(NV_ENC_PRESET_DEFAULT_GUID)
		preset = NV_ENC_PRESET_DEFAULT_GUID;
#else
		preset = NV_ENC_PRESET_P1_GUID; // fastest
#endif
		memset(&m_encConfig, 0, sizeof(m_encConfig)); m_encConfig.version = NV_ENC_CONFIG_VER;
		NV_ENC_PRESET_CONFIG pc{}; pc.version = NV_ENC_PRESET_CONFIG_VER; pc.presetCfg.version = NV_ENC_CONFIG_VER;
		if (m_fn.nvEncGetEncodePresetConfig(m_session, codec, preset, &pc) != NV_ENC_SUCCESS) return false;
		m_encConfig = pc.presetCfg; m_encConfig.rcParams.averageBitRate = m_cfg.bitrateBps ? m_cfg.bitrateBps : 4*1024*1024;
		m_encConfig.rcParams.vbvBufferSize = m_encConfig.rcParams.averageBitRate;
		m_encConfig.frameIntervalP = 1; // no B-frames for low latency
		// GOP / IDR period from keyframeIntervalMs (e.g. Test 2: GOP=15 → keyframeIntervalMs=150 at 100fps)
		if (m_cfg.keyframeIntervalMs > 0 && m_cfg.fps > 0) {
			const uint32_t gopFrames = (m_cfg.keyframeIntervalMs * m_cfg.fps + 500) / 1000u;
			if (gopFrames > 0 && gopFrames <= 0x7FFFFFFF)
				m_encConfig.encodeCodecConfig.h264Config.idrPeriod = gopFrames;
		}
		memset(&m_init, 0, sizeof(m_init)); m_init.version = NV_ENC_INITIALIZE_PARAMS_VER; m_init.encodeGUID = codec; m_init.presetGUID = preset;
		m_init.encodeWidth = m_cfg.width; m_init.encodeHeight = m_cfg.height; m_init.darWidth = m_cfg.width; m_init.darHeight = m_cfg.height;
		m_init.frameRateNum = m_cfg.fps ? m_cfg.fps : 60; m_init.frameRateDen = 1; m_init.enablePTD = 1; m_init.reportSliceOffsets = 0;
#if defined(NV_ENC_TUNING_INFO_LOW_LATENCY)
		m_init.tuningInfo = NV_ENC_TUNING_INFO_LOW_LATENCY;
#endif
		m_init.encodeConfig = &m_encConfig; m_init.maxEncodeWidth = m_cfg.width; m_init.maxEncodeHeight = m_cfg.height;
		return m_fn.nvEncInitializeEncoder(m_session, &m_init) == NV_ENC_SUCCESS;
	}
	bool createBitstreamBuffer(int index) {
		if (index < 0 || index >= kBitstreamBufferCount) return false;
		NV_ENC_CREATE_BITSTREAM_BUFFER c{}; c.version = NV_ENC_CREATE_BITSTREAM_BUFFER_VER;
		if (m_fn.nvEncCreateBitstreamBuffer(m_session, &c) != NV_ENC_SUCCESS) return false;
		m_bitstreams[index] = c.bitstreamBuffer; return true;
	}
	bool createInputTexture() {
		D3D11_TEXTURE2D_DESC d{}; d.Width = m_cfg.width; d.Height = m_cfg.height; d.MipLevels = 1; d.ArraySize = 1;
		d.Format = DXGI_FORMAT_NV12; d.SampleDesc.Count = 1; d.Usage = D3D11_USAGE_DEFAULT; d.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		return SUCCEEDED(m_d3d->CreateTexture2D(&d, nullptr, &m_inputTex));
	}
#endif
};

std::unique_ptr<IEncoder> EncoderFactory::create(const std::string& type) {
	if (type == "none") return std::make_unique<NullEncoder>();
	if (type == "mf") return std::make_unique<MFEncoder>();
	if (type == "nvenc") return std::make_unique<NVENCEncoder>();
	return std::make_unique<NullEncoder>();
}

} // namespace media
} // namespace tienet


