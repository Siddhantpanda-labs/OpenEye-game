#include "tienet/media/decoder.hpp"
#ifdef _WIN32
#include <mfapi.h>
#include <mfidl.h>
#include <mftransform.h>
#include <mferror.h>
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "mf.lib")
#endif
#if defined(TIENET_HAS_MEDIACODEC)
#include <media/NdkMediaCodec.h>
#include <media/NdkMediaExtractor.h>
#include <media/NdkMediaFormat.h>
#endif

namespace tienet {
namespace media {

class MFDecoder final : public IDecoder {
public:
    bool initialize(const DecoderConfig& cfg) override {
        m_cfg = cfg;
#ifdef _WIN32
        if (!s_mfStarted) {
            HRESULT hr = MFStartup(MF_VERSION, MFSTARTUP_NOSOCKET);
            if (FAILED(hr)) return false;
            s_mfStarted = true;
        }
        // For brevity, keep pass-through stub; real impl would locate H.264 decoder MFT and configure it
        m_initialized = true; return true;
#else
        m_initialized = true; return true;
#endif
    }
    std::vector<uint8_t> decode(const std::vector<uint8_t>& bitstream) override {
        if (!m_initialized) return {};
        // Placeholder: pass-through; MF pipeline is sizable to set up
        return bitstream;
    }
    void shutdown() override { m_initialized = false; }
private:
    DecoderConfig m_cfg{}; bool m_initialized = false;
#ifdef _WIN32
    static inline bool s_mfStarted = false;
#endif
};

class AndroidDecoder final : public IDecoder {
public:
    bool initialize(const DecoderConfig& cfg) override {
        m_cfg = cfg; 
#if defined(TIENET_HAS_MEDIACODEC)
        // Minimal skeleton: create decoder by type
        m_codec = AMediaCodec_createDecoderByType(m_cfg.codec.c_str());
        if (!m_codec) return false;
        AMediaFormat* fmt = AMediaFormat_new();
        AMediaFormat_setString(fmt, AMEDIAFORMAT_KEY_MIME, m_cfg.codec.c_str());
        AMediaFormat_setInt32(fmt, AMEDIAFORMAT_KEY_WIDTH, static_cast<int32_t>(m_cfg.width));
        AMediaFormat_setInt32(fmt, AMEDIAFORMAT_KEY_HEIGHT, static_cast<int32_t>(m_cfg.height));
        media_status_t st = AMediaCodec_configure(m_codec, fmt, nullptr, nullptr, 0);
        AMediaFormat_delete(fmt);
        if (st != AMEDIA_OK) { AMediaCodec_delete(m_codec); m_codec = nullptr; return false; }
        st = AMediaCodec_start(m_codec);
        if (st != AMEDIA_OK) { AMediaCodec_delete(m_codec); m_codec = nullptr; return false; }
#endif
        m_initialized = true; 
        return true; 
    }
    std::vector<uint8_t> decode(const std::vector<uint8_t>& bitstream) override {
        if (!m_initialized) return {};
#if defined(TIENET_HAS_MEDIACODEC)
        if (!m_codec) return bitstream;
        ssize_t idx = AMediaCodec_dequeueInputBuffer(m_codec, 0);
        if (idx >= 0) {
            size_t bufSize = 0; uint8_t* buf = AMediaCodec_getInputBuffer(m_codec, static_cast<size_t>(idx), &bufSize);
            if (buf && bufSize >= bitstream.size()) {
                memcpy(buf, bitstream.data(), bitstream.size());
                AMediaCodec_queueInputBuffer(m_codec, static_cast<size_t>(idx), 0, bitstream.size(), 0, 0);
            } else {
                AMediaCodec_queueInputBuffer(m_codec, static_cast<size_t>(idx), 0, 0, 0, 0);
            }
        }
        AMediaCodecBufferInfo info{};
        ssize_t outIdx = AMediaCodec_dequeueOutputBuffer(m_codec, &info, 0);
        std::vector<uint8_t> out;
        if (outIdx >= 0) {
            size_t outSize = 0; uint8_t* outBuf = AMediaCodec_getOutputBuffer(m_codec, static_cast<size_t>(outIdx), &outSize);
            if (outBuf && info.size > 0) {
                out.assign(outBuf + info.offset, outBuf + info.offset + info.size);
            }
            AMediaCodec_releaseOutputBuffer(m_codec, static_cast<size_t>(outIdx), false);
            return out;
        }
        return bitstream; // fallback if no output immediately
#else
        return bitstream;
#endif
    }
    void shutdown() override { 
        m_initialized = false; 
#if defined(TIENET_HAS_MEDIACODEC)
        if (m_codec) { AMediaCodec_stop(m_codec); AMediaCodec_delete(m_codec); m_codec = nullptr; }
#endif
    }
private:
    DecoderConfig m_cfg{}; bool m_initialized = false;
#if defined(TIENET_HAS_MEDIACODEC)
    AMediaCodec* m_codec = nullptr;
#endif
};

std::unique_ptr<IDecoder> DecoderFactory::create(const std::string& type) {
	if (type == "none") return std::make_unique<NullDecoder>();
	if (type == "system") return std::make_unique<MFDecoder>();
	if (type == "android") return std::make_unique<AndroidDecoder>();
	return std::make_unique<NullDecoder>();
}

} // namespace media
} // namespace tienet


