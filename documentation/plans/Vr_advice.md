You’re asking the **right question now**: not “what patch fixes flicker”, but **what is the correct architecture used by professional streaming systems**. The truth is that low-latency video streaming (like VR streaming) requires a **very specific pipeline design**. If one part is wrong, you get flicker, jitter, or latency.

Below is the **industry-grade architecture used by systems like**:

* Parsec
* Moonlight (NVIDIA GameStream client)
* Steam Remote Play
* Virtual Desktop (VR streaming)

I’ll separate **server** and **client** clearly.

---

# 1. Server architecture (professional design)

The server should be **as simple and deterministic as possible**.

### Render → Encode → Send

```
Game Render (GPU)
      ↓
Convert to NV12 (GPU shader)
      ↓
Hardware encoder (NVENC / AMF / QuickSync)
      ↓
Packetize (RTP-style)
      ↓
UDP send
```

### Key rules

#### 1. Render loop controls frame generation

```
render frame
↓
capture frame
↓
encode frame
↓
send immediately
```

No artificial waits.

---

#### 2. Frames must have sequence numbers

Every frame packet must include:

```
frame_id
timestamp
```

Example:

```
[frame_id][timestamp][payload]
```

Why?

UDP can reorder or drop packets.

Professional streaming **never relies on arrival order**.

---

#### 3. Encoder must be constant latency

Use:

```
NVENC low latency preset
```

Important parameters:

```
no B-frames
low-latency mode
constant bitrate
small GOP
```

Example NVENC config:

```
B-frames = 0
GOP = 30–60
lookahead = off
low-latency = true
```

This prevents the encoder from buffering frames internally.

---

#### 4. Do not pace the server to display rate

Server should send as fast as possible:

```
render → encode → send
```

The **client controls presentation timing**.

---

# 2. Network layer (important)

Professional systems **do not send “one UDP packet = one frame”**.

Instead they use **RTP-like framing**.

Example:

```
frame_id
packet_id
packet_count
timestamp
```

Why?

Large frames are fragmented.

This allows:

```
packet loss detection
frame reassembly
late frame discard
```

---

# 3. Client architecture (the most critical part)

The **client must control presentation timing**, not the decoder.

Professional pipeline:

```
Network thread
      ↓
Packet reorder / frame assembly
      ↓
Decode thread
      ↓
Frame queue (latest frame buffer)
      ↓
Display thread (vsync loop)
      ↓
Renderer
```

---

# 4. The golden rule (industry)

Presentation must be **vsync-driven**.

Not decode-driven.

### Correct model

```
decode frame
↓
store as latestFrame
```

Then:

```
every display vsync:
    display latestFrame
```

This guarantees **perfect frame pacing**.

---

# 5. Frame queue design

Professional clients usually keep **1–3 frames max**.

Example:

```
latestDecodedFrame
previousFrame
```

Old frames are dropped.

Why?

Streaming prefers **low latency over perfect frame order**.

---

# 6. Decoder usage (MediaCodec / hardware)

Decoder should run **as fast as possible**.

```
input frame
↓
decode
↓
output frame
↓
store latest frame
```

Decoder **does not decide presentation timing**.

---

# 7. Display pipeline (critical)

Presentation must happen inside the **display vsync loop**.

Example Android pipeline:

```
Choreographer callback (120Hz)
      ↓
get latest decoded frame
      ↓
render frame
```

The render step should **not depend on decode timing**.

---

# 8. Rendering architecture (important)

Professional clients render using **OpenGL / Vulkan**.

Not UI frameworks.

Pipeline:

```
decoded frame texture
↓
OpenGL renderer
↓
draw to screen
```

For VR:

```
draw left eye
draw right eye
```

All in **one GPU pass**.

---

# 9. Why UI frameworks cause problems

Frameworks like Flutter / React Native introduce **another render loop**.

So you get:

```
decoder vsync
Flutter frame
Surface update
```

Multiple clocks = flicker/jitter.

Professional streaming apps avoid this by rendering **directly to SurfaceView or GL surface**.

---

# 10. Frame timing example (120Hz display)

Display timeline:

```
0 ms
8.33 ms
16.66 ms
25 ms
```

Decode may produce frames at:

```
0 ms
7 ms
14 ms
21 ms
```

Display logic:

```
vsync 0   → newest frame
vsync 8.3 → newest frame
vsync 16  → newest frame
```

Frames may be skipped but pacing stays **perfect**.

---

# 11. How professional systems avoid flicker

They guarantee:

```
one presentation decision per vsync
```

Even if no new frame exists.

---

# 12. Typical professional pipeline

### Server

```
render
↓
encode
↓
send
```

### Client

```
network receive
↓
reorder frames
↓
decode
↓
store latest frame
↓
vsync loop
↓
render
```

---

# 13. Most common mistakes (you likely hit some)

1. Presentation triggered by decode
2. No frame sequence numbers
3. One UDP packet = one frame
4. UI framework rendering instead of GL
5. Multiple unsynchronized clocks
6. Too many frame callbacks

Any one of these can cause flicker.

---

# 14. The simplest robust design

### Server

```
render → encode → send
```

### Client

```
receive
↓
decode
↓
latestFrame
↓
vsync loop
↓
render
```

That's it.

---

# 15. Moonlight-style frame pacing (implemented)

The server uses **fixed-cadence stream pacing** (Moonlight / GameStream style):

- **Target stream FPS:** 90 (`Impl::kTargetStreamFps` in `VRStreamingServer.cpp`). Render can run at any rate (e.g. 140 FPS); only one frame is sent every `1/90` s.
- **Scheduler:** `nextSendTime` is advanced by `frameInterval = 1/90` s after each send. If `now < nextSendTime`, the frame is dropped (always send the newest when due). If the scheduler falls behind (`nextSendTime < now`), it is set to `now + frameInterval` to avoid catch-up bursts.
- **GOP:** Optional short GOP (`Impl::kGopFrames = 15`) for resilience; set to `0` for default keyframe interval.

---

# 16. If you want, I can also give you

• a **reference architecture diagram**
• a **minimal streaming client design**
• a **Moonlight-style frame pacing algorithm**

that you can follow **line-by-line** to make your system stable.

Right now you are **very close**, but the architecture still needs one clean final structure.
