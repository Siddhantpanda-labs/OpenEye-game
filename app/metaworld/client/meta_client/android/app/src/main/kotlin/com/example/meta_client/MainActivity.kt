package com.example.meta_client

import android.os.Build
import io.flutter.embedding.android.FlutterActivity
import io.flutter.embedding.engine.FlutterEngine
import io.flutter.plugin.common.MethodChannel

class MainActivity : FlutterActivity() {
    private val channelName = "openeye/vr_h264_decoder"
    private var h264Decoder: H264Decoder? = null

    /** View type for native SurfaceView (flicker test: same vsync pipeline as MediaCodec). */
    val h264SurfaceViewType = "openeye/h264_surface_view"

    private fun getDisplayRefreshRateHz(): Float {
        return if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            display?.refreshRate ?: 60f
        } else {
            @Suppress("DEPRECATION")
            (windowManager.defaultDisplay?.refreshRate?.toFloat() ?: 60f)
        }
    }

    override fun configureFlutterEngine(flutterEngine: FlutterEngine) {
        super.configureFlutterEngine(flutterEngine)
        val textureRegistry = flutterEngine.renderer
        val refreshRateHz = getDisplayRefreshRateHz()
        h264Decoder = H264Decoder(textureRegistry, refreshRateHz)
        h264Decoder?.let { decoder ->
            flutterEngine.platformViewsController.registry.registerViewFactory(
                h264SurfaceViewType,
                H264SurfaceViewFactory(decoder),
            )
        }

        MethodChannel(flutterEngine.dartExecutor.binaryMessenger, channelName).setMethodCallHandler { call, result ->
            when (call.method) {
                "createDecoder" -> {
                    val textureId = h264Decoder?.createDecoder()
                    if (textureId != null) {
                        result.success(textureId)
                    } else {
                        result.error("DECODER_ERROR", "Failed to create H.264 decoder texture", null)
                    }
                }
                "decodeFrame" -> {
                    val data = call.arguments
                    val bytes: ByteArray? = when (data) {
                        is ByteArray -> data
                        is List<*> -> data.map { (it as? Number)?.toInt()?.and(0xFF)?.toByte() ?: 0.toByte() }.toByteArray()
                        else -> null
                    }
                    if (bytes != null && bytes.isNotEmpty()) {
                        h264Decoder?.decodeFrame(bytes)
                    }
                    result.success(null)
                }
                "disposeDecoder" -> {
                    h264Decoder?.disposeDecoder()
                    result.success(null)
                }
                else -> result.notImplemented()
            }
        }
    }

    override fun cleanUpFlutterEngine(flutterEngine: FlutterEngine) {
        h264Decoder?.disposeDecoder()
        h264Decoder = null
        super.cleanUpFlutterEngine(flutterEngine)
    }
}
