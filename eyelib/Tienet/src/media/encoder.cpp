#include "tienet/media/encoder.hpp"
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

namespace tienet {
namespace media {

static inline void mfSafeRelease(IUnknown* p) { if (p) p->Release(); }

class MFEncoder final : public IEncoder {
public:
    bool initialize(const EncoderConfig& cfg) override {
        m_cfg = cfg;
#ifdef _WIN32
        if (!s_mfStarted) {
            HRESULT hr = MFStartup(MF_VERSION, MFSTARTUP_NOSOCKET);
            if (FAILED(hr)) return false;
            s_mfStarted = true;
        }
        IMFActivate** ppActivate = nullptr; UINT32 count = 0;
        HRESULT hr = MFTEnumEx(MFT_CATEGORY_VIDEO_ENCODER, MFT_ENUM_FLAG_SYNCMFT | MFT_ENUM_FLAG_LOCALMFT,
                                nullptr, nullptr, &ppActivate, &count);
        if (FAILED(hr) || count == 0) {
            if (ppActivate) CoTaskMemFree(ppActivate);
            return false;
        }
        hr = ppActivate[0]->ActivateObject(IID_PPV_ARGS(&m_mft));
        for (UINT32 i = 0; i < count; ++i) mfSafeRelease(ppActivate[i]);
        CoTaskMemFree(ppActivate);
        if (FAILED(hr)) return false;

        // Input: NV12
        hr = MFCreateMediaType(&m_inType);
        if (FAILED(hr)) return false;
        hr = m_inType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
        hr = m_inType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_NV12);
        hr = MFSetAttributeSize(m_inType, MF_MT_FRAME_SIZE, m_cfg.width, m_cfg.height);
        hr = MFSetAttributeRatio(m_inType, MF_MT_FRAME_RATE, m_cfg.fps, 1);
        hr = MFSetAttributeRatio(m_inType, MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
        hr = m_mft->SetInputType(0, m_inType, 0);
        if (FAILED(hr)) return false;

        // Output: H264
        hr = MFCreateMediaType(&m_outType);
        if (FAILED(hr)) return false;
        hr = m_outType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
        hr = m_outType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264);
        hr = MFSetAttributeSize(m_outType, MF_MT_FRAME_SIZE, m_cfg.width, m_cfg.height);
        hr = MFSetAttributeRatio(m_outType, MF_MT_FRAME_RATE, m_cfg.fps, 1);
        hr = m_outType->SetUINT32(MF_MT_AVG_BITRATE, m_cfg.bitrateBps);
        hr = m_mft->SetOutputType(0, m_outType, 0);
        if (FAILED(hr)) return false;

        m_mft->ProcessMessage(MFT_MESSAGE_COMMAND_FLUSH, 0);
        m_mft->ProcessMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, 0);
        m_mft->ProcessMessage(MFT_MESSAGE_NOTIFY_START_OF_STREAM, 0);
        m_initialized = true;
        m_sampleTime = 0;
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

        IMFSample* sample = nullptr; IMFMediaBuffer* buffer = nullptr;
        DWORD cb = static_cast<DWORD>(nv12.size());
        HRESULT hr = MFCreateMemoryBuffer(cb, &buffer);
        if (FAILED(hr)) return {};
        BYTE* dst = nullptr; DWORD maxLen = 0, curLen = 0;
        buffer->Lock(&dst, &maxLen, &curLen);
        memcpy(dst, nv12.data(), cb);
        buffer->Unlock();
        buffer->SetCurrentLength(cb);
        hr = MFCreateSample(&sample);
        if (FAILED(hr)) { mfSafeRelease(buffer); return {}; }
        sample->AddBuffer(buffer);
        mfSafeRelease(buffer);
        const LONGLONG frameDur = 10'000'000LL / (m_cfg.fps ? m_cfg.fps : 60);
        sample->SetSampleTime(m_sampleTime);
        sample->SetSampleDuration(frameDur);
        m_sampleTime += frameDur;

        m_mft->ProcessInput(0, sample, 0);
        mfSafeRelease(sample);

        MFT_OUTPUT_STREAM_INFO osi{}; m_mft->GetOutputStreamInfo(0, &osi);
        IMFSample* outSample = nullptr; IMFMediaBuffer* outBuf = nullptr;
        hr = MFCreateMemoryBuffer(osi.cbSize, &outBuf);
        if (FAILED(hr)) return {};
        hr = MFCreateSample(&outSample);
        if (FAILED(hr)) { mfSafeRelease(outBuf); return {}; }
        outSample->AddBuffer(outBuf);
        MFT_OUTPUT_DATA_BUFFER odb{}; DWORD status = 0; odb.dwStreamID = 0; odb.pSample = outSample;
        hr = m_mft->ProcessOutput(0, 1, &odb, &status);
        std::vector<uint8_t> bitstream;
        if (SUCCEEDED(hr)) {
            BYTE* p = nullptr; DWORD len = 0;
            outBuf->Lock(&p, nullptr, &len);
            bitstream.assign(p, p + len);
            outBuf->Unlock();
        }
        mfSafeRelease(outBuf);
        mfSafeRelease(outSample);
        return bitstream;
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

private:
    EncoderConfig m_cfg{};
    bool m_initialized = false;
#ifdef _WIN32
    static inline bool s_mfStarted = false;
    IMFTransform* m_mft = nullptr; IMFMediaType* m_inType = nullptr; IMFMediaType* m_outType = nullptr;
    LONGLONG m_sampleTime = 0;
#endif
};

class NVENCEncoder final : public IEncoder {
public:
	bool initialize(const EncoderConfig& cfg) override {
		m_cfg = cfg;
#if defined(_WIN32) && defined(TIENET_HAS_NVENC)
		m_initialized = false;
		if (!createD3D()) return false;
		if (!loadNvenc()) return false;
		if (!openSession()) return false;
		if (!initEncoder()) return false;
		if (!createBitstreamBuffer()) return false;
		m_initialized = true;
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
		// CPU RGBA->NV12
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

		// Create/update NV12 texture and upload
		if (!m_inputTex) {
			if (!createInputTexture()) return rgbaFrame;
		}
		D3D11_BOX box{}; box.left=0; box.top=0; box.front=0; box.back=1; box.right=w; box.bottom=h;
		m_ctx->UpdateSubresource(m_inputTex, 0, &box, nv12.data(), w, 0);

		// Register, map, encode
		NV_ENC_REGISTER_RESOURCE reg{}; reg.version = NV_ENC_REGISTER_RESOURCE_VER;
		reg.resourceType = NV_ENC_INPUT_RESOURCE_TYPE_DIRECTX; reg.resourceToRegister = m_inputTex;
		reg.width = w; reg.height = h; reg.pitch = w; reg.bufferFormat = NV_ENC_BUFFER_FORMAT_NV12;
		if (m_fn.nvEncRegisterResource(m_session, &reg) != NV_ENC_SUCCESS) return rgbaFrame;
		NV_ENC_MAP_INPUT_RESOURCE map{}; map.version = NV_ENC_MAP_INPUT_RESOURCE_VER; map.registeredResource = reg.registeredResource;
		if (m_fn.nvEncMapInputResource(m_session, &map) != NV_ENC_SUCCESS) { m_fn.nvEncUnregisterResource(m_session, reg.registeredResource); return rgbaFrame; }
		NV_ENC_PIC_PARAMS pic{}; pic.version = NV_ENC_PIC_PARAMS_VER; pic.inputBuffer = map.mappedResource;
		pic.bufferFmt = NV_ENC_BUFFER_FORMAT_NV12; pic.inputWidth = w; pic.inputHeight = h; pic.outputBitstream = m_bitstream; pic.pictureStruct = NV_ENC_PIC_STRUCT_FRAME;
		if (m_fn.nvEncEncodePicture(m_session, &pic) != NV_ENC_SUCCESS) {
			m_fn.nvEncUnmapInputResource(m_session, map.mappedResource);
			m_fn.nvEncUnregisterResource(m_session, reg.registeredResource);
			return rgbaFrame;
		}
		NV_ENC_LOCK_BITSTREAM lock{}; lock.version = NV_ENC_LOCK_BITSTREAM_VER; lock.outputBitstream = m_bitstream; lock.doNotWait = false;
		if (m_fn.nvEncLockBitstream(m_session, &lock) != NV_ENC_SUCCESS) {
			m_fn.nvEncUnmapInputResource(m_session, map.mappedResource);
			m_fn.nvEncUnregisterResource(m_session, reg.registeredResource);
			return rgbaFrame;
		}
		std::vector<uint8_t> out;
		out.assign(static_cast<uint8_t*>(lock.bitstreamBufferPtr), static_cast<uint8_t*>(lock.bitstreamBufferPtr) + lock.bitstreamSizeInBytes);
		m_fn.nvEncUnlockBitstream(m_session, m_bitstream);
		m_fn.nvEncUnmapInputResource(m_session, map.mappedResource);
		m_fn.nvEncUnregisterResource(m_session, reg.registeredResource);
		return out;
#else
		return rgbaFrame;
#endif
	}

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
        pic.inputWidth = m_cfg.width; pic.inputHeight = m_cfg.height; pic.outputBitstream = m_bitstream; pic.pictureStruct = NV_ENC_PIC_STRUCT_FRAME;
        if (m_fn.nvEncEncodePicture(m_session, &pic) != NV_ENC_SUCCESS) {
            m_fn.nvEncUnmapInputResource(m_session, map.mappedResource);
            m_fn.nvEncUnregisterResource(m_session, reg.registeredResource);
            return {};
        }
        NV_ENC_LOCK_BITSTREAM lock{}; lock.version = NV_ENC_LOCK_BITSTREAM_VER; lock.outputBitstream = m_bitstream; lock.doNotWait = false;
        if (m_fn.nvEncLockBitstream(m_session, &lock) != NV_ENC_SUCCESS) {
            m_fn.nvEncUnmapInputResource(m_session, map.mappedResource);
            m_fn.nvEncUnregisterResource(m_session, reg.registeredResource);
            return {};
        }
        std::vector<uint8_t> out;
        out.assign(static_cast<uint8_t*>(lock.bitstreamBufferPtr), static_cast<uint8_t*>(lock.bitstreamBufferPtr) + lock.bitstreamSizeInBytes);
        m_fn.nvEncUnlockBitstream(m_session, m_bitstream);
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
	HMODULE m_nvLib = nullptr; NV_ENCODE_API_FUNCTION_LIST m_fn{}; void* m_session = nullptr; NV_ENC_OUTPUT_PTR m_bitstream = nullptr;
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
		memset(&m_init, 0, sizeof(m_init)); m_init.version = NV_ENC_INITIALIZE_PARAMS_VER; m_init.encodeGUID = codec; m_init.presetGUID = preset;
		m_init.encodeWidth = m_cfg.width; m_init.encodeHeight = m_cfg.height; m_init.darWidth = m_cfg.width; m_init.darHeight = m_cfg.height;
		m_init.frameRateNum = m_cfg.fps ? m_cfg.fps : 60; m_init.frameRateDen = 1; m_init.enablePTD = 1; m_init.reportSliceOffsets = 0;
#if defined(NV_ENC_TUNING_INFO_LOW_LATENCY)
		m_init.tuningInfo = NV_ENC_TUNING_INFO_LOW_LATENCY;
#endif
		m_init.encodeConfig = &m_encConfig; m_init.maxEncodeWidth = m_cfg.width; m_init.maxEncodeHeight = m_cfg.height;
		return m_fn.nvEncInitializeEncoder(m_session, &m_init) == NV_ENC_SUCCESS;
	}
	bool createBitstreamBuffer() {
		NV_ENC_CREATE_BITSTREAM_BUFFER c{}; c.version = NV_ENC_CREATE_BITSTREAM_BUFFER_VER;
		if (m_fn.nvEncCreateBitstreamBuffer(m_session, &c) != NV_ENC_SUCCESS) return false;
		m_bitstream = c.bitstreamBuffer; return true;
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


