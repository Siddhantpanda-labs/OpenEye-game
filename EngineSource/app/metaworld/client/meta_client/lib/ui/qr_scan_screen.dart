import 'package:flutter/material.dart';
import 'package:mobile_scanner/mobile_scanner.dart';

/// Result of scanning a VR server QR code: host and port.
class VRServerScanResult {
  const VRServerScanResult(this.host, this.port);
  final String host;
  final int port;
}

/// Full-screen QR scanner that looks for openeye:// or vr:// URLs.
/// Pops with [VRServerScanResult] on success, or null on back.
class QRScanScreen extends StatefulWidget {
  const QRScanScreen({super.key});

  @override
  State<QRScanScreen> createState() => _QRScanScreenState();
}

class _QRScanScreenState extends State<QRScanScreen> {
  final MobileScannerController _controller = MobileScannerController(
    detectionSpeed: DetectionSpeed.normal,
    facing: CameraFacing.back,
    torchEnabled: false,
  );

  bool _hasScanned = false;

  /// Parse openeye://host:port or vr://host:port. Returns null if invalid.
  static VRServerScanResult? parseVRServerUri(String? value) {
    if (value == null || value.isEmpty) return null;
    final trimmed = value.trim();
    final uri = Uri.tryParse(trimmed);
    if (uri == null) return null;
    final scheme = uri.scheme.toLowerCase();
    if (scheme != 'openeye' && scheme != 'vr') return null;
    final host = uri.host;
    if (host.isEmpty) return null;
    final port = uri.hasPort ? uri.port : 27016;
    if (port <= 0 || port > 65535) return null;
    return VRServerScanResult(host, port);
  }

  void _onDetect(BarcodeCapture capture) {
    if (_hasScanned) return;
    final list = capture.barcodes;
    for (final barcode in list) {
      final raw = barcode.rawValue;
      final result = parseVRServerUri(raw);
      if (result != null) {
        _hasScanned = true;
        if (mounted) Navigator.of(context).pop(result);
        return;
      }
    }
  }

  @override
  void dispose() {
    _controller.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      backgroundColor: Colors.black,
      appBar: AppBar(
        title: const Text('Scan server QR'),
        backgroundColor: Colors.grey[900],
        foregroundColor: Colors.white,
      ),
      body: Stack(
        children: [
          MobileScanner(
            controller: _controller,
            onDetect: _onDetect,
          ),
          Center(
            child: Container(
              width: 260,
              height: 260,
              decoration: BoxDecoration(
                border: Border.all(color: Colors.white54, width: 2),
                borderRadius: BorderRadius.circular(12),
              ),
            ),
          ),
          Positioned(
            bottom: 32,
            left: 24,
            right: 24,
            child: Text(
              'Point at openeye:// or vr:// QR code on the server',
              style: TextStyle(color: Colors.white70, fontSize: 14),
              textAlign: TextAlign.center,
            ),
          ),
        ],
      ),
    );
  }
}
