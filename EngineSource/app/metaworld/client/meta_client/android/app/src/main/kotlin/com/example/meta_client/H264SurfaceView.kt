package com.example.meta_client

import android.content.Context
import android.util.Log
import android.view.SurfaceHolder
import android.view.SurfaceView

/**
 * SurfaceView that provides its Surface to H264Decoder for direct output.
 * Used to test flicker: MediaCodec → Surface → same vsync pipeline (no Flutter Texture sampling).
 */
class H264SurfaceView(
    context: Context,
    private val decoder: H264Decoder,
) : SurfaceView(context), SurfaceHolder.Callback {

    init {
        holder.addCallback(this)
    }

    override fun surfaceCreated(holder: SurfaceHolder) {
        val surface = holder.surface
        if (surface != null && surface.isValid) {
            decoder.setOutputSurface(surface)
            Log.d(TAG, "surfaceCreated: Surface passed to decoder")
        }
    }

    override fun surfaceChanged(holder: SurfaceHolder, format: Int, width: Int, height: Int) {}

    override fun surfaceDestroyed(holder: SurfaceHolder) {
        decoder.setOutputSurface(null)
        Log.d(TAG, "surfaceDestroyed: Surface detached from decoder")
    }

    companion object {
        private const val TAG = "H264SurfaceView"
    }
}
