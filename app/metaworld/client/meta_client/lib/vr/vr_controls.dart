import 'dart:async';
import 'dart:math';
import 'package:flutter/material.dart';
import 'package:sensors_plus/sensors_plus.dart';
import 'package:vector_math/vector_math_64.dart' as math;

/// Stable head tracking with quaternion-only fusion:
/// - gyro integrated in quaternion space (no Euler feedback drift)
/// - gravity correction from accelerometer (tilt stabilization)
/// - stillness bias calibration + online bias adaptation
/// - stillness lock to prevent stop-tail drift
class VRControls {
  final StreamController<Map<String, double>> _headTrackingController =
      StreamController<Map<String, double>>.broadcast();

  StreamSubscription<AccelerometerEvent>? _accelerometerSubscription;
  StreamSubscription<GyroscopeEvent>? _gyroscopeSubscription;

  bool _isInitialized = false;
  final math.Vector3 _headPosition = math.Vector3.zero();

  // Orientation in quaternion space.
  math.Quaternion _q = math.Quaternion.identity();
  // Calibration quaternion: output = qCalib^-1 * q.
  math.Quaternion _qCalib = math.Quaternion.identity();

  // Gyro bias estimation (rad/s).
  math.Vector3 _gyroBias = math.Vector3.zero();
  int _lastGyroTimeUs = 0;

  // Gravity estimate from accelerometer (unit vector in device frame).
  bool _hasAccel = false;
  math.Vector3 _accelUnit = math.Vector3(0, 0, -1);

  // Calibration session.
  bool _isCalibrating = false;
  final List<math.Vector3> _biasSamples = [];
  static const int _biasCalibrationMs = 700;
  Timer? _calibrationTimer;

  // Filter tuning.
  static const double _dtFallbackSec = 0.016;
  static const double _dtMinSec = 0.001;
  static const double _dtMaxSec = 0.05;
  static const double _gyroDeadzoneRadPerSec = 0.012;
  static const double _accelLowPassAlpha = 0.90;
  static const double _accelGravityMin = 8.0;
  static const double _accelGravityMax = 11.6;
  static const double _stillGyroThreshold = 0.09;
  static const double _lockEnterGyroThreshold = 0.05;
  static const double _lockExitGyroThreshold = 0.08;
  static const int _lockEnterFrames = 4;
  static const double _kpAccelStill = 0.06;
  static const double _kpAccelMoving = 0.02;
  static const double _biasAdaptRate = 0.006;
  // Client-to-server frame remap.
  // Symptom fix: mirrored + 180-rotated scene => invert yaw, add 180 deg yaw offset.
  static const double _yawSign = -1.0;
  static const double _pitchSign = 1.0;
  static const double _rollSign = 1.0;
  static const double _yawOffsetRad = pi;
  static const double _pitchOffsetRad = 0.0;
  static const double _rollOffsetRad = 0.0;
  int _stillFrameCount = 0;
  bool _motionLocked = false;

  bool get isInitialized => _isInitialized;
  Stream<Map<String, double>> get headTrackingStream => _headTrackingController.stream;
  bool get isCalibrating => _isCalibrating;

  Future<void> initialize() async {
    try {
      _gyroscopeSubscription = gyroscopeEventStream().listen(_handleGyroscope);
      _accelerometerSubscription = accelerometerEventStream().listen(_handleAccelerometer);
      _isInitialized = true;
    } catch (e) {
      throw Exception('Failed to initialize VR controls: $e');
    }
  }

  void _handleGyroscope(GyroscopeEvent event) {
    if (!_isInitialized) return;

    final nowUs = DateTime.now().microsecondsSinceEpoch;
    double dtSec = _dtFallbackSec;
    if (_lastGyroTimeUs != 0) {
      dtSec = (nowUs - _lastGyroTimeUs) / 1000000.0;
      if (dtSec < _dtMinSec || dtSec > _dtMaxSec) dtSec = _dtFallbackSec;
    }
    _lastGyroTimeUs = nowUs;

    final raw = math.Vector3(event.x.toDouble(), event.y.toDouble(), event.z.toDouble());

    if (_isCalibrating) {
      _biasSamples.add(math.Vector3.copy(raw));
      return;
    }

    final corrected = math.Vector3(
      raw.x - _gyroBias.x,
      raw.y - _gyroBias.y,
      raw.z - _gyroBias.z,
    );
    _applyDeadzone(corrected);

    final gyroMag = corrected.length;
    final isStill = gyroMag < _stillGyroThreshold;
    if (gyroMag < _lockEnterGyroThreshold) {
      _stillFrameCount++;
      if (_stillFrameCount >= _lockEnterFrames) {
        _motionLocked = true;
      }
    } else if (gyroMag > _lockExitGyroThreshold) {
      _stillFrameCount = 0;
      _motionLocked = false;
    }

    // Slow online bias adaptation when device is still to reduce long-term drift.
    if (isStill) {
      _gyroBias = _gyroBias * (1.0 - _biasAdaptRate) + raw * _biasAdaptRate;
    }

    if (!_motionLocked) {
      // Gravity correction in quaternion space (no axis-frame drift).
      if (_hasAccel) {
        final worldDown = math.Vector3(0, 0, -1);
        final predictedDownInDevice = _q.conjugated().rotated(worldDown);
        if (predictedDownInDevice.length2 > 0) {
          predictedDownInDevice.normalize();
          // sensors_plus accelerometer reports proper acceleration direction.
          // Negate to get gravity/down direction for attitude correction.
          final measuredDownInDevice = -math.Vector3.copy(_accelUnit);
          measuredDownInDevice.normalize();
          final error = _cross(predictedDownInDevice, measuredDownInDevice);
          final kp = isStill ? _kpAccelStill : _kpAccelMoving;
          corrected.x += kp * error.x;
          corrected.y += kp * error.y;
          corrected.z += kp * error.z;
        }
      }

      final omegaMag = corrected.length;
      if (omegaMag > 1e-7) {
        final axis = corrected / omegaMag;
        final dq = math.Quaternion.axisAngle(axis, omegaMag * dtSec);
        _q = _q * dq;
        _q.normalize();
      }
    }

    _sendHeadTrackingData();
  }

  void _handleAccelerometer(AccelerometerEvent event) {
    if (!_isInitialized) return;

    final ax = event.x.toDouble();
    final ay = event.y.toDouble();
    final az = event.z.toDouble();
    final g = sqrt(ax * ax + ay * ay + az * az);
    if (g < _accelGravityMin || g > _accelGravityMax) {
      _hasAccel = false;
      return;
    }

    final nx = ax / g;
    final ny = ay / g;
    final nz = az / g;

    final measured = math.Vector3(nx, ny, nz);
    if (!_hasAccel) {
      _accelUnit = measured;
      _hasAccel = true;
      return;
    }
    _accelUnit = _accelUnit * _accelLowPassAlpha + measured * (1.0 - _accelLowPassAlpha);
    if (_accelUnit.length2 > 0) {
      _accelUnit.normalize();
      _hasAccel = true;
    }
  }

  void _applyDeadzone(math.Vector3 v) {
    if (v.x.abs() < _gyroDeadzoneRadPerSec) v.x = 0.0;
    if (v.y.abs() < _gyroDeadzoneRadPerSec) v.y = 0.0;
    if (v.z.abs() < _gyroDeadzoneRadPerSec) v.z = 0.0;
  }

  double _normalizeAngle(double radians) {
    var a = radians;
    while (a > pi) a -= 2.0 * pi;
    while (a < -pi) a += 2.0 * pi;
    return a;
  }

  math.Vector3 _cross(math.Vector3 a, math.Vector3 b) {
    return math.Vector3(
      a.y * b.z - a.z * b.y,
      a.z * b.x - a.x * b.z,
      a.x * b.y - a.y * b.x,
    );
  }

  void _quaternionToEuler(math.Quaternion q, void Function(double yaw, double pitch, double roll) out) {
    final x = q.x;
    final y = q.y;
    final z = q.z;
    final w = q.w;

    final sinp = 2.0 * (w * y - z * x);
    final pitch = sinp.abs() >= 1.0 ? sinp.sign * (pi / 2.0) : asin(sinp);
    final sinRCosp = 2.0 * (w * x + y * z);
    final cosRCosp = 1.0 - 2.0 * (x * x + y * y);
    final roll = atan2(sinRCosp, cosRCosp);
    final sinYCosp = 2.0 * (w * z + x * y);
    final cosYCosp = 1.0 - 2.0 * (y * y + z * z);
    final yaw = atan2(sinYCosp, cosYCosp);

    out(yaw, pitch, roll);
  }

  void _sendHeadTrackingData() {
    final outQ = _qCalib.conjugated() * _q;
    double yawRaw = 0.0;
    double pitchRaw = 0.0;
    double rollRaw = 0.0;
    _quaternionToEuler(outQ, (y, p, r) {
      yawRaw = _normalizeAngle(y);
      pitchRaw = _normalizeAngle(p);
      rollRaw = _normalizeAngle(r);
    });
    final yaw = _normalizeAngle(_yawSign * yawRaw + _yawOffsetRad);
    final pitch = _normalizeAngle(_pitchSign * pitchRaw + _pitchOffsetRad);
    final roll = _normalizeAngle(_rollSign * rollRaw + _rollOffsetRad);

    _headTrackingController.add({
      'yaw': yaw,
      'pitch': pitch,
      'roll': roll,
      'x': _headPosition.x,
      'y': _headPosition.y,
      'z': _headPosition.z,
    });
  }

  /// Calibrate while device is still: estimate bias + set current orientation as forward.
  void calibrate() {
    if (_isCalibrating) return;
    _isCalibrating = true;
    _biasSamples.clear();

    _calibrationTimer?.cancel();
    _calibrationTimer = Timer(const Duration(milliseconds: _biasCalibrationMs), () {
      _calibrationTimer = null;
      if (_biasSamples.length >= 10) {
        var sum = math.Vector3.zero();
        for (final s in _biasSamples) {
          sum += s;
        }
        _gyroBias = sum / _biasSamples.length.toDouble();
      }
      _qCalib = math.Quaternion.copy(_q);
      _isCalibrating = false;
      _biasSamples.clear();
      _lastGyroTimeUs = 0;
      _stillFrameCount = 0;
      _motionLocked = false;
    });
  }

  Map<String, double> getHeadTrackingData() {
    final outQ = _qCalib.conjugated() * _q;
    double yawRaw = 0.0;
    double pitchRaw = 0.0;
    double rollRaw = 0.0;
    _quaternionToEuler(outQ, (y, p, r) {
      yawRaw = _normalizeAngle(y);
      pitchRaw = _normalizeAngle(p);
      rollRaw = _normalizeAngle(r);
    });
    final yaw = _normalizeAngle(_yawSign * yawRaw + _yawOffsetRad);
    final pitch = _normalizeAngle(_pitchSign * pitchRaw + _pitchOffsetRad);
    final roll = _normalizeAngle(_rollSign * rollRaw + _rollOffsetRad);
    return {
      'yaw': yaw,
      'pitch': pitch,
      'roll': roll,
      'x': _headPosition.x,
      'y': _headPosition.y,
      'z': _headPosition.z,
    };
  }

  void handleTouchInput(Offset position, Size screenSize) {
    final normalizedX = (position.dx / screenSize.width) * 2.0 - 1.0;
    final normalizedY = (position.dy / screenSize.height) * 2.0 - 1.0;
    _headTrackingController.add({
      'touch_x': normalizedX,
      'touch_y': normalizedY,
      'touch_active': 1.0,
    });
  }

  Widget build(BuildContext context) {
    if (!_isInitialized) {
      return const SizedBox.shrink();
    }

    return GestureDetector(
      onTapDown: (details) {
        if (context.size != null) handleTouchInput(details.localPosition, context.size!);
      },
      onPanUpdate: (details) {
        if (context.size != null) handleTouchInput(details.localPosition, context.size!);
      },
      child: Container(
        color: Colors.transparent,
        child: Stack(
          children: [
            Positioned(
              top: 20,
              left: 20,
              child: GestureDetector(
                onTap: calibrate,
                child: Container(
                  padding: const EdgeInsets.all(8),
                  decoration: BoxDecoration(
                    color: _isCalibrating
                        ? Colors.orange.withValues(alpha: 0.8)
                        : Colors.blue.withValues(alpha: 0.7),
                    borderRadius: BorderRadius.circular(4),
                  ),
                  child: Text(
                    _isCalibrating ? 'Hold still...' : 'Calibrate',
                    style: const TextStyle(color: Colors.white, fontSize: 12),
                  ),
                ),
              ),
            ),
            Positioned(
              bottom: 20,
              left: 20,
              child: Container(
                padding: const EdgeInsets.all(8),
                decoration: BoxDecoration(
                  color: Colors.black.withValues(alpha: 0.7),
                  borderRadius: BorderRadius.circular(4),
                ),
                child: StreamBuilder<Map<String, double>>(
                  stream: headTrackingStream,
                  builder: (context, snapshot) {
                    if (!snapshot.hasData) {
                      return const Text(
                        'Head Tracking: Initializing...',
                        style: TextStyle(color: Colors.white, fontSize: 10),
                      );
                    }
                    final data = snapshot.data!;
                    return Text(
                      'Yaw: ${(data['yaw'] ?? 0).toStringAsFixed(2)}\n'
                      'Pitch: ${(data['pitch'] ?? 0).toStringAsFixed(2)}\n'
                      'Roll: ${(data['roll'] ?? 0).toStringAsFixed(2)}',
                      style: const TextStyle(color: Colors.white, fontSize: 10),
                    );
                  },
                ),
              ),
            ),
          ],
        ),
      ),
    );
  }

  void dispose() {
    _calibrationTimer?.cancel();
    _calibrationTimer = null;
    _accelerometerSubscription?.cancel();
    _gyroscopeSubscription?.cancel();
    _headTrackingController.close();
    _isInitialized = false;
  }
}
