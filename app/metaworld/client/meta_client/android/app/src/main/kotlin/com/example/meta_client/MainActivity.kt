package com.example.meta_client

import android.os.Build
import io.flutter.embedding.android.FlutterActivity
import io.flutter.embedding.engine.FlutterEngine
import io.flutter.plugin.common.EventChannel
import io.flutter.plugin.common.MethodChannel

class MainActivity : FlutterActivity() {
    private val channelName = "openeye/vr_h264_decoder"
    private val rotationVectorChannelName = "openeye/rotation_vector"
    private val rotationVectorEventChannelName = "openeye/rotation_vector_events"
    private var h264Decoder: H264Decoder? = null
    private var rotationVectorTracker: RotationVectorTracker? = null

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

        MethodChannel(flutterEngine.dartExecutor.binaryMessenger, rotationVectorChannelName).setMethodCallHandler { call, result ->
            if (rotationVectorTracker == null) rotationVectorTracker = RotationVectorTracker(this)
            val tracker = rotationVectorTracker
            when (call.method) {
                "isAvailable" -> result.success(tracker?.isAvailable == true)
                "startTracking" -> {
                    tracker?.startTracking()
                    result.success(null)
                }
                "stopTracking" -> {
                    tracker?.stopTracking()
                    result.success(null)
                }
                "getQuaternion" -> {
                    if (tracker == null || !tracker.isAvailable) {
                        result.error("UNAVAILABLE", "Rotation vector not available", null)
                    } else {
                        val q = tracker.getQuaternion()
                        result.success(listOf(q[0].toDouble(), q[1].toDouble(), q[2].toDouble(), q[3].toDouble()))
                    }
                }
                else -> result.notImplemented()
            }
        }

        EventChannel(flutterEngine.dartExecutor.binaryMessenger, rotationVectorEventChannelName).setStreamHandler(
            object : EventChannel.StreamHandler {
                override fun onListen(arguments: Any?, events: EventChannel.EventSink?) {
                    if (rotationVectorTracker == null) rotationVectorTracker = RotationVectorTracker(this@MainActivity)
                    val tracker = rotationVectorTracker ?: return
                    if (events == null) return
                    tracker.onPose = { q, timestampUs ->
                        events.success(listOf(q[0].toDouble(), q[1].toDouble(), q[2].toDouble(), q[3].toDouble(), timestampUs.toDouble()))
                    }
                    tracker.startTracking()
                }
                override fun onCancel(arguments: Any?) {
                    rotationVectorTracker?.onPose = null
                    rotationVectorTracker?.stopTracking()
                }
            }
        )
    }

    override fun cleanUpFlutterEngine(flutterEngine: FlutterEngine) {
        rotationVectorTracker?.stopTracking()
        rotationVectorTracker = null
        h264Decoder?.disposeDecoder()
        h264Decoder = null
        super.cleanUpFlutterEngine(flutterEngine)
    }
}
