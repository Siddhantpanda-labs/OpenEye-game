package com.example.meta_client

import android.content.Context
import android.view.View
import io.flutter.plugin.common.StandardMessageCodec
import io.flutter.plugin.platform.PlatformView
import io.flutter.plugin.platform.PlatformViewFactory

/**
 * Creates the native SurfaceView for H.264 output (flicker test: same vsync pipeline).
 */
class H264SurfaceViewFactory(
    private val decoder: H264Decoder,
) : PlatformViewFactory(StandardMessageCodec.INSTANCE) {

    override fun create(context: Context, viewId: Int, args: Any?): PlatformView {
        decoder.prepareForSurfaceView()
        val surfaceView = H264SurfaceView(context, decoder)
        return object : PlatformView {
            override fun getView(): View = surfaceView
            override fun dispose() {}
        }
    }
}
