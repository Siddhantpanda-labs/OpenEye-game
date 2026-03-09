package com.example.meta_client

import android.content.Context
import android.hardware.Sensor
import android.hardware.SensorEvent
import android.hardware.SensorEventListener
import android.hardware.SensorManager
import android.view.Surface
import android.view.WindowManager
import kotlin.math.sqrt

/**
 * Android Game Rotation Vector (gyro+accel fusion, no magnetometer) with display-rotation remap.
 * Push-based: on each sensor event invokes onPose(q, timestampUs) for low-latency VR tracking.
 */
class RotationVectorTracker(private val context: Context) : SensorEventListener {

    private val sensorManager: SensorManager? =
        context.getSystemService(Context.SENSOR_SERVICE) as? SensorManager
    private val windowManager: WindowManager? =
        context.getSystemService(Context.WINDOW_SERVICE) as? WindowManager

    private val rotationVectorSensor: Sensor? =
        sensorManager?.getDefaultSensor(Sensor.TYPE_GAME_ROTATION_VECTOR)

    private val R = FloatArray(9)
    private val Rremap = FloatArray(9)
    private val qRemap = FloatArray(4).apply {
        set(3, 1f) // identity quaternion (x,y,z,w) = (0,0,0,1)
    }

    @Volatile
    private var isTracking = false

    /** Called on each sensor event: (x, y, z, w), timestamp in microseconds (monotonic). */
    @Volatile
    var onPose: ((FloatArray, Long) -> Unit)? = null

    val isAvailable: Boolean
        get() = rotationVectorSensor != null

    fun startTracking() {
        if (!isTracking && rotationVectorSensor != null && sensorManager != null) {
            sensorManager.registerListener(
                this,
                rotationVectorSensor,
                SensorManager.SENSOR_DELAY_FASTEST
            )
            isTracking = true
        }
    }

    fun stopTracking() {
        if (isTracking && sensorManager != null) {
            sensorManager.unregisterListener(this)
            isTracking = false
        }
        onPose = null
    }

    /** Returns quaternion (x, y, z, w) in display-remapped right-handed coordinates. */
    fun getQuaternion(): FloatArray = qRemap

    override fun onSensorChanged(event: SensorEvent) {
        if (event.sensor.type != Sensor.TYPE_GAME_ROTATION_VECTOR) return

        SensorManager.getRotationMatrixFromVector(R, event.values)

        val rotation = windowManager?.defaultDisplay?.rotation ?: Surface.ROTATION_0
        when (rotation) {
            Surface.ROTATION_0 -> SensorManager.remapCoordinateSystem(
                R, SensorManager.AXIS_X, SensorManager.AXIS_Y, Rremap
            )
            Surface.ROTATION_90 -> SensorManager.remapCoordinateSystem(
                R, SensorManager.AXIS_Y, SensorManager.AXIS_MINUS_X, Rremap
            )
            Surface.ROTATION_180 -> SensorManager.remapCoordinateSystem(
                R, SensorManager.AXIS_MINUS_X, SensorManager.AXIS_MINUS_Y, Rremap
            )
            Surface.ROTATION_270 -> SensorManager.remapCoordinateSystem(
                R, SensorManager.AXIS_MINUS_Y, SensorManager.AXIS_X, Rremap
            )
            else -> R.copyInto(Rremap)
        }

        matrixToQuaternion(Rremap, qRemap)

        val timestampUs = event.timestamp / 1000L
        onPose?.invoke(qRemap, timestampUs)
    }

    override fun onAccuracyChanged(sensor: Sensor, accuracy: Int) {}

    private fun matrixToQuaternion(m: FloatArray, qOut: FloatArray) {
        val t = m[0] + m[4] + m[8]
        when {
            t > 0f -> {
                val s = sqrt(t + 1f) * 2f
                qOut[3] = 0.25f * s
                qOut[0] = (m[7] - m[5]) / s
                qOut[1] = (m[2] - m[6]) / s
                qOut[2] = (m[3] - m[1]) / s
            }
            m[0] > m[4] && m[0] > m[8] -> {
                val s = sqrt(1f + m[0] - m[4] - m[8]) * 2f
                qOut[3] = (m[7] - m[5]) / s
                qOut[0] = 0.25f * s
                qOut[1] = (m[1] + m[3]) / s
                qOut[2] = (m[2] + m[6]) / s
            }
            m[4] > m[8] -> {
                val s = sqrt(1f + m[4] - m[0] - m[8]) * 2f
                qOut[3] = (m[2] - m[6]) / s
                qOut[0] = (m[1] + m[3]) / s
                qOut[1] = 0.25f * s
                qOut[2] = (m[5] + m[7]) / s
            }
            else -> {
                val s = sqrt(1f + m[8] - m[0] - m[4]) * 2f
                qOut[3] = (m[3] - m[1]) / s
                qOut[0] = (m[2] + m[6]) / s
                qOut[1] = (m[5] + m[7]) / s
                qOut[2] = 0.25f * s
            }
        }
        val norm = sqrt(qOut[0] * qOut[0] + qOut[1] * qOut[1] + qOut[2] * qOut[2] + qOut[3] * qOut[3])
        if (norm > 0f) {
            qOut[0] /= norm
            qOut[1] /= norm
            qOut[2] /= norm
            qOut[3] /= norm
        }
    }
}
