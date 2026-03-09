# VR streaming refactor – steps and FPS checks

Refactors are applied in order. After each step, run the player with a VR client connected and note **Stream FPS** in the overlay (OpenEye VR Server panel) or the main **FPS** in the runtime overlay.

---

## Baseline (before refactor)

- Full-window readback (e.g. 1920×1080) as float → CPU float→byte → CPU downscale to 960×540 → stb JPEG (quality 75) → UDP.
- **Expected: ~6–7 FPS** when client connected.

---

## Step 1 + 2 + 3 (current build)

**Done in one pass:**

1. **Streaming FBO** – Render output is blitted to a **960×540 RGBA8** render target; readback is from this FBO only (no full-window read, no CPU downscale).
2. **Byte readback** – For RGBA8 RTs, `glReadPixels(..., GL_UNSIGNED_BYTE)` is used (no float transfer, no float→byte loop).
3. **JPEG quality 50** – `SendFrame(..., 50)` for faster encode and smaller packets.

**How to measure:** Run OpenEyePlayer with streaming, connect the phone, open the **OpenEye VR Server** overlay panel. Read **Stream FPS** (and/or main **FPS**).  
**Expected: ~20–30 FPS** (depending on CPU).

---

## Step 4 (optional – not yet done)

**Replace stb_image_write with libjpeg-turbo** for JPEG encoding.

- Add libjpeg-turbo as a dependency (or use a vcpkg/fetch content).
- In `app/streaming/FrameEncoder.cpp`, use libjpeg-turbo’s memory API instead of stb.
- Rebuild, connect client, measure **Stream FPS** again.  
**Expected: ~30–45 FPS.**

---

## Step 5: Async readback (PBO) — done

**Double-buffered PBO so readback is not synchronized with 3D rendering:**

- Use pixel buffer objects so readback doesn’t stall the render loop.
- Encode JPEG on a worker thread; send the *previous* frame each time (one frame latency).
- **Expected: ~45–60 FPS** if the rest of the pipeline is light.

---

## Files touched in Step 1+2+3 and Step 5 (PBO)

| Area | Files |
|------|--------|
| RHI | `eyelib/render/rhi/RHI.h` – PBO methods (default no-op); `OpenGL/DeviceGL.cpp` – byte readback, CreatePixelPackBuffer, ReadPixelsToPBO, MapPixelPackBuffer |
| API | `api/render/RenderPipeline.h/.cpp` – `ReadPixelsFromRenderTarget()`, CreatePixelPackBuffer, DestroyPixelPackBuffer, ReadPixelsToPBO, MapPixelPackBuffer |
| Player | `app/player/WorldRender.h/.cpp` – PrepareStreamingFrame(), ReadStreamingPixels(), GetStreamingPixelsFromPreviousFrame(), IssueStreamingReadback(), double-buffered PBOs, shutdown cleanup |
| GameRuntime | `app/player/GameRuntime.cpp` – async path (GetStreamingPixelsFromPreviousFrame → send → PrepareStreamingFrame → IssueStreamingReadback), sync fallback when no PBO |
| Overlay | `app/player/Overlay.cpp` – show **Stream FPS** in VR server panel |

---

## Quick test

1. Build: `cmake --build build --target OpenEyePlayer` (with `OPENEYE_BUILD_STREAMING=ON`).
2. Run player, load a scene (e.g. VR scene).
3. On phone: connect via QR or IP:27016.
4. On PC: check **Stream FPS** in the “OpenEye VR Server” overlay (top-right when a device is connected).

---

## H.264 / NVENC pipeline (implemented)

When **OPENEYE_STREAMING_H264** is ON (default), the server can send H.264 instead of JPEG on Windows:

- **Build:** Tienet is added as a dependency; its MF (Media Foundation) encoder is used for H.264. Option **OPENEYE_STREAMING_NVENC** enables NVENC when the NVIDIA Video Codec SDK is available.
- **Protocol:** Each UDP frame is `[1 byte format][4 byte size BE][payload]`. Format 0 = JPEG, 1 = H.264. Legacy clients that only expect `[4 byte size][payload]` still work (treated as JPEG).
- **Server:** `VRStreamingServer::SendFrame` uses Tienet's encoder (MF or NVENC) when built with H.264 and on Windows; otherwise falls back to JPEG. Encoder is created on first frame (960×540, 4 Mbps, 60 fps).
- **Client (Flutter):** `udp_client.dart` parses the 5-byte header and emits `VrFrameData(format, data)`. JPEG (format 0) is passed to `Image.memory`. H.264 (format 1) uses a **texture-based pipeline**: platform channel `openeye/vr_h264_decoder` → Android `H264Decoder` (MediaCodec → SurfaceTexture) → Flutter `Texture` widget; no per-frame image bytes over the channel. Methods: `createDecoder` (returns texture ID), `decodeFrame` (feeds Annex B NAL bytes), `disposeDecoder`.

- **Server H.264 format (for client decoder):** Annex B NAL units (start code `0x00 0x00 0x00 0x01` or `0x00 0x00 0x01`). Resolution 960×540. Client decoder configures MediaCodec with 960×540 and feeds raw UDP payload per frame.

**Files:** `app/streaming/VRStreamingServer.cpp`, `app/streaming/CMakeLists.txt`, `lib/networking/udp_client.dart`, `lib/ui/vr_screen.dart`, `lib/vr/vr_renderer.dart`, `lib/vr/stereo_renderer.dart`, `android/.../MainActivity.kt`, `android/.../H264Decoder.kt`.

---

## Encoder thread + async bitstream (Vr_advice Fix 1 & 2)

- **Fix 1:** Encoding runs on a dedicated thread. Render thread only copies frame into a pool buffer and pushes to a queue; encoder thread pops, calls `SendFrame`, recycles buffer. No blocking of the render loop. Pool size 3.
- **Fix 2:** Tienet NVENC uses 3 bitstream buffers and `NvEncLockBitstream(doNotWait=1)` first; if busy, retries with `doNotWait=0`. Reduces stalls when encoder keeps up.

---

## Fix 3 & 4 plumbing (NV12 and direct D3D11 texture)

- **Tienet encoder API:** `IEncoder::encodeFromNV12(nv12Frame)` and `IEncoder::encodeFromD3D11NV12(d3d11TextureNV12)` (Windows). NVENC encoder implements both: NV12 path skips CPU RGBA→NV12; D3D11 path encodes from an external NV12 texture (no upload).
- **Server:** `VRStreamingServer::SendFrameNV12(nv12, width, height)` encodes and sends H.264 when the H.264 encoder is active.
- **Full Fix 3 (GPU NV12):** Not implemented. Would require a pipeline pass (shader that samples streaming RT and writes Y/UV to a buffer or two RTs, then readback). Caller would then use `SendFrameNV12` with that buffer.
- **Full Fix 4 (direct texture):** Not implemented. Would require OpenGL–D3D11 interop (e.g. WGL_NV_DX_interop): render or copy into a D3D11 NV12 texture, then pass it to `encodeFromD3D11NV12`.
