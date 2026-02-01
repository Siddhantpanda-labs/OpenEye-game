# Tienet (eyelib/Tienet) - Code Review Report

Date: 2026-02-01

This report summarizes the current state of `eyelib/Tienet`, based on reading headers, implementations, build config, examples, and the project plan in `eyelib/Tienet/plan-for-tienet.md`.

## 1) What Tienet is (current intent)

Tienet is a standalone UDP-focused networking library intended for:
- multi-client realtime transport (sessions, channels, reliability tiers)
- optional VR streaming (frame transport + gyro/touch)
- optional Flutter integration
- optional compression, congestion control, and basic crypto

The codebase is organized as a self-contained library with headers under `eyelib/Tienet/include/` and implementations under `eyelib/Tienet/src/`.

## 2) Build and configuration

Build target:
- `add_library(Tienet STATIC ...)` in `eyelib/Tienet/CMakeLists.txt`

Feature toggles:
- `TIENET_ENABLE_OPENSSL` (AES-GCM)
- `TIENET_ENABLE_SQLITE` (persistence)
- `TIENET_ENABLE_NVENC` (NVIDIA encoder)
- `TIENET_ENABLE_ANDROID_MEDIACODEC`
- `TIENET_ENABLE_CUDA` (optional NVENC interop)

Windows links:
- `ws2_32`, Media Foundation (`mfplat`, `mfuuid`, `mf`, `ole32`)

Examples and tests:
- Examples: `basic_server.cpp`, `basic_client.cpp`, `discovery_example.cpp`, `media_transport_example.cpp`, `vr_server.cpp`
- Tests: `tests/test_tienet.cpp`

## 3) High-level architecture (as implemented)

Core transport:
- `Socket` abstraction (`tienet/core/socket.hpp`)
- `Packet` system + `Protocol` state machine
- Reliability with ACK/NACK, retransmit buffer, SACK window
- Fragmentation + reassembly
- Optional pacing, rate control, FEC, and PMTU discovery

Multi-client management:
- `SessionManager` + `Session` wrappers (heartbeat, reconnect, stats)

Multiplexed channels:
- `Channel` + `ChannelManager` with delivery modes and priorities

Server / client:
- `server::Server` + `server::UDPServer`
- `client::Client` + `client::UDPClient`

VR extensions:
- `vr::VRServer` + `vr::VRClient`
- Frame transport, compression, encoder/decoder hooks
- Gyro and touch input channels
- Jitter/reorder buffer on client

Discovery:
- UDP discovery server/client in `tienet/discovery`

Security:
- AES-GCM via OpenSSL (if enabled)
- NullCipher fallback (development)
- ACL and token scaffolding

## 4) Feature highlights in code

Protocol:
- Pacing + adaptive bitrate, rate controller, and MTU probing
- FEC grouping on send and receive paths (XOR parity)
- Per-packet encryption with AAD as header, nonce = sequence

Congestion control:
- Multiple algorithms implemented (CUBIC, BBR, Vegas, Reno)
- Adaptive controller that can switch between algorithms

VR streaming:
- Windows Media Foundation encoder path
- NVENC encoder path (Windows)
- Android MediaCodec decoder skeleton
- ABR binding from transport bitrate to encoder bitrate

## 5) Observed gaps or risk points

These are based on direct reading and may be intentional placeholders:

1) Fragment reassembly is simplified
   - `Protocol::reassemblePacket()` currently returns only the first fragment.
   - The newer fragment group path exists in `processData`, but the legacy method is still incomplete.

2) Channel send path is not fully wired
   - `ChannelManager::update()` only calls `Channel::update()`, which does not actually call a send callback.
   - `ChannelManager::initialize()` stores `SendCallback`, but it is never used in the `Channel` path.

3) Channel compression is a placeholder
   - `compressData()` / `decompressData()` simply copy data.

4) Decoder paths are stubbed
   - Windows MF decoder path is a pass-through stub.
   - Android decoder is skeletal and does not return full decoded frames reliably.

5) Crypto fallback is not secure
   - When OpenSSL is off, AES-GCM falls back to XOR with 0xAA (development only).

6) Encoder keyframe control is TODO
   - `forceKeyframe()` is a placeholder for both MF and NVENC.

## 6) Plan vs code alignment

The plan file (`eyelib/Tienet/plan-for-tienet.md`) claims:
- Core transport hardening and session multiplexing are complete.
- VR streaming pipeline is in progress.
- QA, benchmarks, and docs are pending.

The code matches this generally:
- Transport features are present (pacing, FEC, PMTU).
- VR encode paths exist, decode paths are not fully implemented.
- Tests/examples are limited to basic flows.

## 7) Recommendations (short list)

If you want Tienet to be production-grade inside OpenEye, the most practical next steps are:

1) Complete fragment reassembly or remove legacy code paths.
2) Wire `ChannelManager` send callbacks so channel payloads actually transmit.
3) Replace channel compression placeholders with real LZ4/Zstd integration (or remove until ready).
4) Implement MF decoder or clearly document decoder as a stub.
5) Formalize the crypto fallback as "dev-only" and guard in config.
6) Add tests around reliability, FEC recovery, and rate control.

## 8) Summary

Tienet is a capable foundation for UDP-based realtime transport with optional VR streaming.
The core architecture is solid (protocol, sessions, channels, discovery), but several paths are still partial or stubbed.
The most important correctness risks today are fragment reassembly and the channel send path not being connected to transport.

If you want, I can follow this report with a prioritized fix plan or a diff-based patch set.
