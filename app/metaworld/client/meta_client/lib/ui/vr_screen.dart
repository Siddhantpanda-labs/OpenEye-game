import 'dart:async';
import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import '../vr/vr_renderer.dart';
import '../vr/vr_controls.dart';
import '../networking/udp_client.dart';

class VRScreen extends StatefulWidget {
  const VRScreen({super.key, this.udpClient});

  /// Connected UDP client from ConnectionScreen. If null, a new client is used (e.g. direct /vr route).
  final UDPClient? udpClient;

  @override
  State<VRScreen> createState() => _VRScreenState();
}

class _VRScreenState extends State<VRScreen> {
  late final UDPClient _udpClient;
  final VRRenderer _vrRenderer = VRRenderer();
  final VRControls _vrControls = VRControls();
  
  bool _isInitialized = false;
  String _status = 'Initializing VR...';
  int _framesReceived = 0;
  int _fpsStartMs = 0;
  /// Time (ms since epoch) when we last received a frame; 0 = none yet. Used for latency display.
  int _lastFrameReceiveMs = 0;
  /// Overlay stats: 1s timer only. Decode path never triggers setState.
  Timer? _statsTimer;
  /// Min interval between JPEG-driven rebuilds (ms).
  int _lastJpegRebuildMs = 0;
  static const int _jpegRebuildIntervalMs = 33;

  @override
  void initState() {
    super.initState();
    _udpClient = widget.udpClient ?? UDPClient();
    _initializeVR();
  }

  Future<void> _initializeVR() async {
    try {
      // Set landscape orientation for VR
      await SystemChrome.setPreferredOrientations([
        DeviceOrientation.landscapeLeft,
        DeviceOrientation.landscapeRight,
      ]);
      
      // Hide system UI for immersive experience
      SystemChrome.setEnabledSystemUIMode(SystemUiMode.immersive);
      
      // Initialize VR renderer
      await _vrRenderer.initialize();
      // One-time only: build view tree when H.264 output is ready. Never per frame.
      _vrRenderer.onH264TextureReady = () => setState(() {});

      // Initialize VR controls (gyroscope, etc.)
      await _vrControls.initialize();
      
      // Set up frame streaming (decode path never calls setState)
      _udpClient.frameStream.listen(_handleFrameData);
      
      // Set up head tracking
      _vrControls.headTrackingStream.listen(_handleHeadTracking);

      // Overlay stats: single clock, 1s interval. Keeps presentation vsync-driven.
      _statsTimer = Timer.periodic(const Duration(seconds: 1), (_) {
        if (mounted) setState(() {});
      });
      
      setState(() {
        _isInitialized = true;
        _status = 'VR Ready - Streaming frames...';
      });
      
    } catch (e) {
      setState(() {
        _status = 'VR Initialization failed: $e';
      });
    }
  }

  /// Handle incoming frames. H.264: decode only (vsync in native). JPEG: throttled setState for display.
  void _handleFrameData(VrFrameData frame) {
    if (!_isInitialized) return;
    final now = DateTime.now().millisecondsSinceEpoch;
    if (_fpsStartMs == 0) _fpsStartMs = now;
    _framesReceived++;
    _lastFrameReceiveMs = now;

    if (frame.format == VrFrameData.formatJpeg) {
      _vrRenderer.renderFrame(frame.data);
      final now = DateTime.now().millisecondsSinceEpoch;
      if (now - _lastJpegRebuildMs >= _jpegRebuildIntervalMs && mounted) {
        _lastJpegRebuildMs = now;
        setState(() {});
      }
    } else if (frame.format == VrFrameData.formatH264) {
      _vrRenderer.decodeAndRenderH264(frame.data);
    }
  }

  /// Time since last frame received (ms). 0 if no frame yet.
  int get _latencyMs {
    if (_lastFrameReceiveMs == 0) return 0;
    return DateTime.now().millisecondsSinceEpoch - _lastFrameReceiveMs;
  }

  double get _fps {
    if (_fpsStartMs == 0 || _framesReceived == 0) return 0.0;
    final elapsedSec = (DateTime.now().millisecondsSinceEpoch - _fpsStartMs) / 1000.0;
    return elapsedSec > 0 ? _framesReceived / elapsedSec : 0.0;
  }

  void _handleHeadTracking(Map<String, double> trackingData) {
    final yaw = trackingData['yaw'] ?? 0.0;
    final pitch = trackingData['pitch'] ?? 0.0;
    final roll = trackingData['roll'] ?? 0.0;
    final timestampUs = trackingData['timestamp_us'];
    _udpClient.sendGyro(yaw, pitch, roll, timestampUs);
  }

  void _exitVR() {
    SystemChrome.setEnabledSystemUIMode(SystemUiMode.manual, 
        overlays: SystemUiOverlay.values);
    Navigator.of(context).pop();
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      backgroundColor: Colors.black,
      body: Stack(
        children: [
          // VR Renderer
          if (_isInitialized)
            Positioned.fill(
              child: _vrRenderer.build(context),
            ),
          
          // VR Controls Overlay
          if (_isInitialized)
            Positioned.fill(
              child: _vrControls.build(context),
            ),
          
          // Status Overlay
          Positioned(
            top: 20,
            left: 20,
            child: Container(
              padding: const EdgeInsets.all(12),
              decoration: BoxDecoration(
                color: Colors.black.withValues(alpha: 0.7),
                borderRadius: BorderRadius.circular(8),
              ),
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Text(
                    _status,
                    style: const TextStyle(
                      color: Colors.white,
                      fontSize: 14,
                    ),
                  ),
                  if (_isInitialized) ...[
                    const SizedBox(height: 4),
                    Text(
                      'Latency: ${_latencyMs} ms',
                      style: const TextStyle(
                        color: Colors.green,
                        fontSize: 12,
                      ),
                    ),
                    Text(
                      'Stream FPS: ${_fps.toStringAsFixed(1)}',
                      style: const TextStyle(
                        color: Colors.blue,
                        fontSize: 12,
                      ),
                    ),
                  ],
                ],
              ),
            ),
          ),
          
          // Exit Button
          Positioned(
            top: 20,
            right: 20,
            child: GestureDetector(
              onTap: _exitVR,
              child: Container(
                padding: const EdgeInsets.all(12),
                decoration: BoxDecoration(
                  color: Colors.red.withValues(alpha: 0.8),
                  borderRadius: BorderRadius.circular(8),
                ),
                child: const Icon(
                  Icons.close,
                  color: Colors.white,
                  size: 24,
                ),
              ),
            ),
          ),
          
          // VR Instructions
          if (_isInitialized)
            Positioned(
              bottom: 20,
              left: 20,
              right: 20,
              child: Container(
                padding: const EdgeInsets.all(16),
                decoration: BoxDecoration(
                  color: Colors.black.withValues(alpha: 0.7),
                  borderRadius: BorderRadius.circular(8),
                ),
                child: const Text(
                  'Move your device to look around in VR. Tap the red X to exit.',
                  style: TextStyle(
                    color: Colors.white,
                    fontSize: 14,
                  ),
                  textAlign: TextAlign.center,
                ),
              ),
            ),
        ],
      ),
    );
  }

  @override
  void dispose() {
    _statsTimer?.cancel();
    _statsTimer = null;
    _vrRenderer.dispose();
    _vrControls.dispose();
    _udpClient.disconnect();
    super.dispose();
  }
}
