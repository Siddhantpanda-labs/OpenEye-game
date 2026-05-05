package com.siddhant.vr;

import android.content.Context;
import android.hardware.Sensor;
import android.hardware.SensorEvent;
import android.hardware.SensorEventListener;
import android.hardware.SensorManager;
import android.view.Display;
import android.view.Surface;
import android.view.WindowManager;

public class HeadTracker implements SensorEventListener {

    private static SensorManager sensorManager;
    private static Sensor rotationVectorSensor;

    // Raw matrix from rotation vector, and remapped matrix according to display rotation
    private static final float[] R = new float[9];
    private static final float[] Rremap = new float[9];

    // Quaternion (x, y, z, w) in ANDROID (right-handed) coordinates AFTER remap
    private static final float[] qRemap = new float[4];

    private static int lastRotation = Surface.ROTATION_0;

    private static WindowManager wm;

    // ---- Public API ----

    public static void startTracking(Context context)
    {
        if (sensorManager != null) return;

        sensorManager = (SensorManager) context.getSystemService(Context.SENSOR_SERVICE);
        rotationVectorSensor = sensorManager.getDefaultSensor(Sensor.TYPE_GAME_ROTATION_VECTOR);

        wm = (WindowManager) context.getSystemService(Context.WINDOW_SERVICE);
        if (rotationVectorSensor != null) {
            sensorManager.registerListener(new HeadTracker(),
                    rotationVectorSensor,
                    SensorManager.SENSOR_DELAY_FASTEST);
        }
    }

    public static void stopTracking()
    {
        if (sensorManager != null) {
            sensorManager.unregisterListener((SensorEventListener) new HeadTracker());
            sensorManager = null;
        }
    }

    // Unity calls this every frame
    public static float[] getQuaternion()
    {
        // Returns right-handed quaternion in phone’s current landscape/portrait basis
        return qRemap;
    }

    // ---- SensorEventListener ----

    @Override
    public void onSensorChanged(SensorEvent event)
    {
        if (event.sensor.getType() != Sensor.TYPE_GAME_ROTATION_VECTOR) return;

        // 1) Rotation vector → rotation matrix (phone’s *natural* portrait basis)
        SensorManager.getRotationMatrixFromVector(R, event.values);

        // 2) Remap matrix to match the CURRENT display rotation (0/90/180/270)
        final int rotation = (wm != null) ? wm.getDefaultDisplay().getRotation() : Surface.ROTATION_0;

        // We want: Landscape-Left to behave naturally for VR
        // Base axes for portrait: X-right, Y-up, Z-out-of-screen (toward user).
        // Remap to display:
        switch (rotation) {
            case Surface.ROTATION_0: // Portrait
                SensorManager.remapCoordinateSystem(R,
                        SensorManager.AXIS_X,       // X -> X
                        SensorManager.AXIS_Y,       // Y -> Y
                        Rremap);
                break;

            case Surface.ROTATION_90: // Landscape-Left (USB/volume buttons usually up)
                SensorManager.remapCoordinateSystem(R,
                        SensorManager.AXIS_Y,       // Portrait Y -> Landscape X
                        SensorManager.AXIS_MINUS_X, // Portrait -X -> Landscape Y
                        Rremap);
                break;

            case Surface.ROTATION_180: // Portrait upside-down
                SensorManager.remapCoordinateSystem(R,
                        SensorManager.AXIS_MINUS_X, // -X -> X
                        SensorManager.AXIS_MINUS_Y, // -Y -> Y
                        Rremap);
                break;

            case Surface.ROTATION_270: // Landscape-Right
                SensorManager.remapCoordinateSystem(R,
                        SensorManager.AXIS_MINUS_Y, // -Y -> X
                        SensorManager.AXIS_X,       //  X -> Y
                        Rremap);
                break;
        }

        lastRotation = rotation;

        // 3) Convert the **remapped** rotation matrix to a quaternion (right-handed, Android)
        // Android doesn't provide a direct "from matrix to quaternion" helper, so we do it here:
        matrixToQuaternion(Rremap, qRemap);
    }

    @Override
    public void onAccuracyChanged(Sensor sensor, int accuracy) {}

    // ---- Utility: 3x3 rotation matrix → quaternion (x,y,z,w), right-handed ----
    private static void matrixToQuaternion(float[] m, float[] qOut)
    {
        // m is 3x3 in row-major (SensorManager uses this convention for getRotationMatrixFromVector)
        float t = m[0] + m[4] + m[8]; // trace

        if (t > 0.0f) {
            float s = (float)Math.sqrt(t + 1.0f) * 2f;
            qOut[3] = 0.25f * s;
            qOut[0] = (m[7] - m[5]) / s;
            qOut[1] = (m[2] - m[6]) / s;
            qOut[2] = (m[3] - m[1]) / s;
        } else if ((m[0] > m[4]) && (m[0] > m[8])) {
            float s = (float)Math.sqrt(1.0f + m[0] - m[4] - m[8]) * 2f;
            qOut[3] = (m[7] - m[5]) / s;
            qOut[0] = 0.25f * s;
            qOut[1] = (m[1] + m[3]) / s;
            qOut[2] = (m[2] + m[6]) / s;
        } else if (m[4] > m[8]) {
            float s = (float)Math.sqrt(1.0f + m[4] - m[0] - m[8]) * 2f;
            qOut[3] = (m[2] - m[6]) / s;
            qOut[0] = (m[1] + m[3]) / s;
            qOut[1] = 0.25f * s;
            qOut[2] = (m[5] + m[7]) / s;
        } else {
            float s = (float)Math.sqrt(1.0f + m[8] - m[0] - m[4]) * 2f;
            qOut[3] = (m[3] - m[1]) / s;
            qOut[0] = (m[2] + m[6]) / s;
            qOut[1] = (m[5] + m[7]) / s;
            qOut[2] = 0.25f * s;
        }

        // Normalize (safety)
        float norm = (float)Math.sqrt(qOut[0]*qOut[0] + qOut[1]*qOut[1] + qOut[2]*qOut[2] + qOut[3]*qOut[3]);
        if (norm > 0f) {
            qOut[0] /= norm; qOut[1] /= norm; qOut[2] /= norm; qOut[3] /= norm;
        }
    }
}
