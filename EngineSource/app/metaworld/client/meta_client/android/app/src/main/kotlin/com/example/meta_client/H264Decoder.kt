package com.example.meta_client

import android.graphics.SurfaceTexture
import android.media.MediaCodec
import android.media.MediaFormat
import android.os.Handler
import android.os.HandlerThread
import android.os.Looper
import android.util.Log
import android.view.Choreographer
import android.view.Surface
import io.flutter.view.TextureRegistry
import java.util.concurrent.atomic.AtomicBoolean

/**
 * H.264 decoder that outputs to a Flutter texture or SurfaceView.
 * Decoding runs on a dedicated background thread; only the latest incoming frame is kept.
 * Presentation is driven by a continuous Choreographer vsync loop: one presentation decision
 * per display refresh; the newest decoded frame (if any) is released at each vsync.
 *
 * Server sends: Annex B NAL units (0x00 0x00 0x00 0x01); resolution 960x540.
 */
class H264Decoder(
    private val textureRegistry: TextureRegistry,
    private val displayRefreshRateHz: Float = 60f,
) {
    companion object {
        private const val TAG = "H264Decoder"
        private const val MIME = "video/avc"
        private const val DEFAULT_WIDTH = 960
        private const val DEFAULT_HEIGHT = 540
    }

    private var surfaceTextureEntry: TextureRegistry.SurfaceTextureEntry? = null
    private var surface: Surface? = null
    /** When non-null, decoder outputs to this Surface (SurfaceView) instead of SurfaceTexture. */
    @Volatile private var surfaceFromView: Surface? = null
    private var codec: MediaCodec? = null
    private val running = AtomicBoolean(false)
    private var outputWidth: Int = DEFAULT_WIDTH
    private var outputHeight: Int = DEFAULT_HEIGHT

    private var decoderThread: HandlerThread? = null
    private var decoderHandler: Handler? = null
    private var mainHandler: Handler? = null

    @Volatile private var latestFrame: ByteArray? = null
    private val frameLock = Object()

    /** Pending buffer to release at next vsync (decoder thread sets, vsync loop consumes). */
    private val pendingLock = Object()
    private var pendingRelease: Pair<MediaCodec, Int>? = null

    /** True when the continuous vsync loop has been started (main thread only). */
    @Volatile private var vsyncLoopStarted = false

    fun createDecoder(): Long? {
        if (surfaceTextureEntry != null) {
            Log.w(TAG, "createDecoder: already created, returning existing id")
            return surfaceTextureEntry!!.id()
        }
        return try {
            startDecoderThreadIfNeeded()
            val entry = textureRegistry.createSurfaceTexture()
            surfaceTextureEntry = entry
            val surfaceTexture = entry.surfaceTexture()
            surfaceTexture.setDefaultBufferSize(DEFAULT_WIDTH, DEFAULT_HEIGHT)
            surface = Surface(surfaceTexture)
            outputWidth = DEFAULT_WIDTH
            outputHeight = DEFAULT_HEIGHT
            running.set(true)
            mainHandler?.post(::startVsyncLoop)

            Log.d(TAG, "createDecoder: texture id=${entry.id()}")
            entry.id()
        } catch (e: Exception) {
            Log.e(TAG, "createDecoder failed", e)
            surfaceTextureEntry = null
            surface = null
            null
        }
    }

    /**
     * Accepts H.264 data from the main thread. Replaces any pending frame (drops old ones)
     * and posts decode work to the background thread. Returns immediately.
     */
    fun decodeFrame(h264Data: ByteArray?) {
        if (h264Data == null || h264Data.isEmpty()) return
        if (!running.get()) return
        synchronized(frameLock) {
            latestFrame = h264Data
        }
        decoderHandler?.post(::processNextFrame)
    }

    private fun processNextFrame() {
        val data: ByteArray
        synchronized(frameLock) {
            data = latestFrame ?: return
            latestFrame = null
        }
        if (!running.get()) return
        if (getOutputSurface() == null) return
        ensureCodec()
        val c = codec ?: return
        try {
            val inputBufferIndex = c.dequeueInputBuffer(0)
            if (inputBufferIndex >= 0) {
                val inputBuffer = c.getInputBuffer(inputBufferIndex) ?: return
                inputBuffer.clear()
                inputBuffer.put(data)
                c.queueInputBuffer(
                    inputBufferIndex,
                    0,
                    data.size,
                    System.nanoTime() / 1000,
                    0
                )
            }
            drainOutput(c)
        } catch (e: Exception) {
            Log.w(TAG, "processNextFrame failed", e)
        }
    }

    private fun ensureCodec() {
        if (codec != null) return
        val outputSurface = getOutputSurface() ?: return
        try {
            val format = MediaFormat.createVideoFormat(MIME, DEFAULT_WIDTH, DEFAULT_HEIGHT)
            format.setInteger(MediaFormat.KEY_FRAME_RATE, 60)
            val c = MediaCodec.createDecoderByType(MIME)
            c.configure(format, outputSurface, null, 0)
            c.start()
            codec = c
            Log.d(TAG, "MediaCodec started, output to Surface")
        } catch (e: Exception) {
            Log.e(TAG, "ensureCodec failed", e)
            codec = null
        }
    }

    /**
     * Drain all decoded output buffers. Release older frames without displaying (drop);
     * present only the newest frame at exact vsync via Choreographer callback.
     */
    private fun drainOutput(c: MediaCodec) {
        val bufferInfo = MediaCodec.BufferInfo()
        var lastIdx = -1
        var lastFlags = 0
        while (running.get()) {
            val idx = c.dequeueOutputBuffer(bufferInfo, 0)
            when {
                idx == MediaCodec.INFO_OUTPUT_FORMAT_CHANGED -> {
                    val format = c.outputFormat
                    outputWidth = format.getInteger(MediaFormat.KEY_WIDTH)
                    outputHeight = format.getInteger(MediaFormat.KEY_HEIGHT)
                    Log.d(TAG, "Output format changed: ${outputWidth}x$outputHeight")
                }
                idx == MediaCodec.INFO_TRY_AGAIN_LATER -> break
                idx >= 0 -> {
                    if (lastIdx >= 0) {
                        c.releaseOutputBuffer(lastIdx, false)
                    }
                    lastIdx = idx
                    lastFlags = bufferInfo.flags
                }
                else -> break
            }
        }
        if (lastIdx >= 0) {
            val previous = synchronized(pendingLock) {
                val old = pendingRelease
                pendingRelease = Pair(c, lastIdx)
                old
            }
            previous?.let { (codec, idx) -> codec.releaseOutputBuffer(idx, false) }
            if (lastFlags and MediaCodec.BUFFER_FLAG_END_OF_STREAM != 0) return
        }
    }

    /** Starts the continuous vsync loop (runs on main thread). Called once when decoder becomes active. */
    private fun startVsyncLoop() {
        if (vsyncLoopStarted || !running.get()) return
        vsyncLoopStarted = true
        Choreographer.getInstance().postFrameCallback(::onVsyncFrame)
    }

    /**
     * Runs every vsync. Presents the newest decoded frame (if any); re-registers for next vsync.
     * Guarantees one presentation decision per display refresh.
     */
    private fun onVsyncFrame(frameTimeNanos: Long) {
        val p = synchronized(pendingLock) {
            val x = pendingRelease
            pendingRelease = null
            x
        }
        if (p != null) {
            try {
                p.first.releaseOutputBuffer(p.second, frameTimeNanos)
            } catch (_: Exception) {}
        }
        if (running.get()) {
            Choreographer.getInstance().postFrameCallback(::onVsyncFrame)
        }
    }

    fun disposeDecoder() {
        running.set(false)
        vsyncLoopStarted = false
        decoderHandler?.removeCallbacksAndMessages(null)
        mainHandler?.removeCallbacksAndMessages(null)
        synchronized(pendingLock) { pendingRelease = null }
        decoderThread?.quitSafely()
        decoderThread?.join(500)
        decoderThread = null
        decoderHandler = null
        mainHandler = null
        try {
            codec?.apply {
                try { stop(); release() } catch (_: Exception) {}
            }
            codec = null
        } catch (_: Exception) {}
        surfaceFromView = null
        surface?.release()
        surface = null
        surfaceTextureEntry?.release()
        surfaceTextureEntry = null
        Log.d(TAG, "disposeDecoder done")
    }

    fun getTextureId(): Long? = surfaceTextureEntry?.id()

    /** Prepare for SurfaceView output (no Flutter texture). Call before showing the SurfaceView. */
    fun prepareForSurfaceView() {
        if (decoderThread != null) return
        startDecoderThreadIfNeeded()
        running.set(true)
        mainHandler?.post(::startVsyncLoop)
        Log.d(TAG, "prepareForSurfaceView: decoder thread ready, waiting for Surface")
    }

    /** Set output Surface from SurfaceView (call from surfaceCreated). */
    fun setOutputSurface(outputSurface: Surface?) {
        surfaceFromView = outputSurface
        if (outputSurface != null) Log.d(TAG, "setOutputSurface: Surface attached")
    }

    private fun startDecoderThreadIfNeeded() {
        if (decoderThread != null) return
        val thread = HandlerThread("H264DecoderThread").also { it.start() }
        decoderThread = thread
        decoderHandler = Handler(thread.looper)
        mainHandler = Handler(Looper.getMainLooper())
    }

    /** Output surface: SurfaceView if set, else Flutter texture Surface. */
    private fun getOutputSurface(): Surface? = surfaceFromView ?: surface
}
