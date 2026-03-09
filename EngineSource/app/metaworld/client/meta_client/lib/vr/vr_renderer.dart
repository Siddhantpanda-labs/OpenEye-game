import 'dart:async';
import 'dart:typed_data';
import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:vector_math/vector_math_64.dart' as math;
import 'stereo_renderer.dart';

class VRRenderer {
  final StereoRenderer _stereoRenderer = StereoRenderer();
  bool _isInitialized = false;
  Uint8List? _currentFrame;
  final int _frameWidth = 1280;
  final int _frameHeight = 720;

  /// Called once when H.264 output is ready so the host can build the view tree once.
  /// Must not be called per frame — presentation is vsync-driven in native; Flutter must not rebuild on decode.
  VoidCallback? onH264TextureReady;

  int? _h264TextureId;
  bool _h264Creating = false;
  /// True after we've notified the host once that H.264 view is ready (no further setState on decode).
  bool _h264ViewNotified = false;
  /// Use native SurfaceView for H.264 (vsync-driven, same pipeline as MediaCodec).
  static const bool _useH264SurfaceView = true;

  // VR rendering parameters
  double _ipd = 0.064; // Inter-pupillary distance in meters
  double _fov = 90.0; // Field of view in degrees
  math.Vector3 _headPosition = math.Vector3.zero();
  math.Vector3 _headRotation = math.Vector3.zero();

  bool get isInitialized => _isInitialized;

  /// Initialize the VR renderer
  Future<void> initialize() async {
    try {
      await _stereoRenderer.initialize();
      _isInitialized = true;
    } catch (e) {
      throw Exception('Failed to initialize VR renderer: $e');
    }
  }

  /// Render a frame in VR (JPEG or other displayable image bytes)
  void renderFrame(Uint8List frameData) {
    if (!_isInitialized) return;
    _currentFrame = frameData;
    _stereoRenderer.updateFrame(frameData, _frameWidth, _frameHeight);
  }

  /// Decode H.264 and render via native texture or SurfaceView (no per-frame bytes over channel).
  /// Presentation is vsync-driven in native; we never trigger Flutter rebuild per frame.
  void decodeAndRenderH264(Uint8List h264Data) {
    if (!_isInitialized) return;
    if (_useH264SurfaceView) {
      _stereoRenderer.setUseH264SurfaceView(true);
      if (!_h264ViewNotified) {
        _h264ViewNotified = true;
        onH264TextureReady?.call();
      }
      _H264DecoderService.decodeFrame(h264Data);
      return;
    }
    if (_h264TextureId != null) {
      _H264DecoderService.decodeFrame(h264Data);
      return;
    }
    if (_h264Creating) return;
    _h264Creating = true;
    _H264DecoderService.createDecoder().then((int? textureId) {
      _h264Creating = false;
      if (textureId == null) return;
      _h264TextureId = textureId;
      _stereoRenderer.setH264Texture(textureId);
      if (!_h264ViewNotified) {
        _h264ViewNotified = true;
        onH264TextureReady?.call();
      }
      _H264DecoderService.decodeFrame(h264Data);
    }).catchError((_) {
      _h264Creating = false;
    });
  }

  /// Update head tracking data
  void updateHeadTracking(math.Vector3 position, math.Vector3 rotation) {
    _headPosition = position;
    _headRotation = rotation;
    // _stereoRenderer.updateHeadTracking(position, rotation);
  }

  /// Set VR parameters
  void setVRParameters({
    double? ipd,
    double? fov,
  }) {
    if (ipd != null) _ipd = ipd;
    if (fov != null) _fov = fov;
    
    // _stereoRenderer.setVRParameters(ipd: _ipd, fov: _fov);
  }

  /// Build the VR renderer widget
  Widget build(BuildContext context) {
    if (!_isInitialized) {
      return const Center(
        child: CircularProgressIndicator(),
      );
    }

    return _stereoRenderer.build(context);
  }

  /// Dispose resources
  void dispose() {
    _h264ViewNotified = false;
    if (_h264TextureId != null || _useH264SurfaceView) {
      _H264DecoderService.disposeDecoder();
      _h264TextureId = null;
    }
    _stereoRenderer.dispose();
    _isInitialized = false;
  }
}

/// Simple frame decoder for RGBA data
class FrameDecoder {
  static Image? decodeRGBAFrame(Uint8List data, int width, int height) {
    try {
      // Convert RGBA bytes to Flutter Image
      // This is a simplified version - in production you'd want proper image decoding
      return null; // Placeholder - would need proper implementation
    } catch (e) {
      return null;
    }
  }
}

/// H.264 texture-based decoder (Android MediaCodec → SurfaceTexture → Flutter Texture).
/// No per-frame image bytes over the channel.
class _H264DecoderService {
  static const _channel = MethodChannel('openeye/vr_h264_decoder');

  static Future<int?> createDecoder() async {
    final result = await _channel.invokeMethod<int>('createDecoder');
    return result;
  }

  static void decodeFrame(Uint8List h264Data) {
    _channel.invokeMethod<void>('decodeFrame', h264Data);
  }

  static Future<void> disposeDecoder() async {
    await _channel.invokeMethod<void>('disposeDecoder');
  }
}

/// VR distortion correction
class VRDistortion {
  static math.Vector2 applyDistortion(math.Vector2 uv, math.Vector2 lensCenter, 
      math.Vector2 scale, math.Vector2 scaleIn, math.Vector4 distortion) {
    
    final offset = uv - lensCenter;
    final theta = offset.length;
    final thetaSq = theta * theta;
    final theta4 = thetaSq * thetaSq;
    final theta6 = theta4 * thetaSq;
    
    final factor = 1.0 + distortion.x * thetaSq + distortion.y * theta4 + distortion.z * theta6;
    final rTheta = factor * theta;
    
    final rThetaSq = rTheta * rTheta;
    final rTheta4 = rThetaSq * rThetaSq;
    // final rTheta6 = rTheta4 * rThetaSq;
    
    final distortionFactor = 1.0 + distortion.w * rThetaSq;
    
    return lensCenter + offset * distortionFactor * scale.x;
  }
}
