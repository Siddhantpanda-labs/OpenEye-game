import 'dart:typed_data';
import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:vector_math/vector_math_64.dart' as math;

class StereoRenderer {
  bool _isInitialized = false;
  Uint8List? _currentFrame;
  /// When set, display H.264 decoded stream via native texture (no per-frame bytes).
  int? _h264TextureId;
  /// When true, use native SurfaceView for H.264 (flicker test: same vsync as MediaCodec).
  bool _useH264SurfaceView = false;

  bool get isInitialized => _isInitialized;

  Future<void> initialize() async {
    _isInitialized = true;
  }

  /// Update frame data (JPEG bytes from server)
  void updateFrame(Uint8List frameData, [int width = 0, int height = 0]) {
    _currentFrame = frameData;
  }

  /// Use native H.264 texture for display (left/right show same texture).
  void setH264Texture(int textureId) {
    _h264TextureId = textureId;
    _currentFrame = null;
  }

  /// Use native SurfaceView for H.264 (flicker test: no Flutter texture sampling).
  void setUseH264SurfaceView(bool use) {
    _useH264SurfaceView = use;
    if (use) _currentFrame = null;
  }

  /// Clear H.264 texture mode (e.g. when switching back to JPEG or disposing).
  void clearH264Texture() {
    _h264TextureId = null;
    _useH264SurfaceView = false;
  }

  /// Update head tracking (reserved for future stereo/distortion)
  void updateHeadTracking(math.Vector3 position, math.Vector3 rotation) {}

  /// Set VR parameters (reserved for future use)
  void setVRParameters({double? ipd, double? fov}) {}

  /// Build the stereo renderer widget
  Widget build(BuildContext context) {
    if (!_isInitialized) {
      return const Center(
        child: CircularProgressIndicator(),
      );
    }

    // H.264 SurfaceView test: native view, same vsync as MediaCodec (no Flutter texture).
    if (_useH264SurfaceView) {
      return SizedBox.expand(
        child: AndroidView(
          viewType: 'openeye/h264_surface_view',
        ),
      );
    }
    // H.264 texture path: one Flutter Texture (may flicker due to two clocks).
    if (_h264TextureId != null) {
      return SizedBox.expand(
        child: Texture(textureId: _h264TextureId!),
      );
    }

    // JPEG path: display streamed image bytes
    if (_currentFrame != null && _currentFrame!.isNotEmpty) {
      return Row(
        children: [
          Expanded(
            child: Image.memory(
              _currentFrame!,
              fit: BoxFit.cover,
              gaplessPlayback: true,
            ),
          ),
          Expanded(
            child: Image.memory(
              _currentFrame!,
              fit: BoxFit.cover,
              gaplessPlayback: true,
            ),
          ),
        ],
      );
    }

    return _buildPlaceholder();
  }

  Widget _buildPlaceholder() {
    return Container(
      color: Colors.grey[800],
      alignment: Alignment.center,
      child: const Text(
        'Waiting for VR frames...',
        style: TextStyle(color: Colors.white, fontSize: 18),
      ),
    );
  }

  /// Dispose resources
  void dispose() {
    clearH264Texture();
    _currentFrame = null;
    _isInitialized = false;
  }
}

