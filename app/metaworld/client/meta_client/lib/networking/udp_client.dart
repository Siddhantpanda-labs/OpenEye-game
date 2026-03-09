import 'dart:async';
import 'dart:typed_data';
import 'dart:io';
import 'package:flutter/foundation.dart';

/// Frame format from server. 0 = JPEG, 1 = H.264.
/// Industry-grade: frameId and timestampUs for reorder / late-frame discard (0 = legacy).
class VrFrameData {
  const VrFrameData({
    required this.format,
    required this.data,
    this.frameId = 0,
    this.timestampUs = 0,
  });
  final int format;
  final Uint8List data;
  final int frameId;
  final int timestampUs;
  static const int formatJpeg = 0;
  static const int formatH264 = 1;
}

class UDPClient {
  RawDatagramSocket? _socket;
  StreamSubscription<RawSocketEvent>? _subscription;
  final StreamController<VrFrameData> _frameController = StreamController<VrFrameData>.broadcast();
  final StreamController<String> _discoveredServersController = StreamController<String>.broadcast();
  
  String? _serverHost;
  int? _serverPort;
  bool _isConnected = false;
  /// Last emitted frame_id (32-bit); -1 = none. Used to drop late/reordered frames.
  int _lastEmittedFrameId = -1;
  static const int _kFrameHeaderSize = 17; // [1 format][4 frame_id][8 timestamp_us][4 size]
  static const int _kFragmentHeaderSize = 21; // + [2 chunk_idx][2 chunk_count]
  static const int _kAssemblyTimeoutMs = 250;
  final Map<int, _FrameAssembly> _assemblies = <int, _FrameAssembly>{};

  // Getters
  bool get isConnected => _isConnected;
  Stream<VrFrameData> get frameStream => _frameController.stream;
  Stream<String> get discoveredServers => _discoveredServersController.stream;

  /// Connect to a discovered VR server
  Future<bool> connectToDiscoveredServer(String serverEntry) async {
    try {
      // Parse server entry: "ServerName (IP:Port)"
      final match = RegExp(r'(.+?)\s+\((.+?):(\d+)\)').firstMatch(serverEntry);
      if (match == null) {
        debugPrint('❌ Invalid server entry format: $serverEntry');
        return false;
      }
      
      final serverName = match.group(1)!;
      final serverHost = match.group(2)!;
      final serverPort = int.parse(match.group(3)!);
      
      debugPrint('🔗 Connecting to server: $serverName');
      debugPrint('📡 Server address: $serverHost:$serverPort');
      
      return await connect(serverHost, serverPort);
      
    } catch (e) {
      debugPrint('❌ Failed to connect to discovered server: $e');
      return false;
    }
  }

  /// Connect to the VR streaming server
  Future<bool> connect(String host, int port) async {
    try {
      _serverHost = host;
      _serverPort = port;
      
      // Create UDP socket
      _socket = await RawDatagramSocket.bind(InternetAddress.anyIPv4, 0);
      
      // Set up event listener
      _subscription = _socket!.listen(
        _handleSocketEvent,
        onError: (error) {
          debugPrint('UDP Socket Error: $error');
          _isConnected = false;
        },
        onDone: () {
          debugPrint('UDP Socket closed');
          _isConnected = false;
        },
      );

      // Send connection request to server and wait for response
      final connectionMessage = _createConnectionMessage();
      final serverAddress = InternetAddress(host);
      _socket!.send(connectionMessage, serverAddress, port);
      
      // Wait for server response (with timeout)
      debugPrint('Waiting for server response...');
      final responseReceived = await _waitForServerResponse();
      
      if (responseReceived) {
        _isConnected = true;
        debugPrint('Connected to VR server at $host:$port');
        return true;
      } else {
        _isConnected = false;
        debugPrint('No response from server at $host:$port');
        return false;
      }
      
    } catch (e) {
      debugPrint('Failed to connect to VR server: $e');
      _isConnected = false;
      return false;
    }
  }

  /// Handle incoming socket events
  void _handleSocketEvent(RawSocketEvent event) {
    switch (event) {
      case RawSocketEvent.read:
        _handleIncomingData();
        break;
      case RawSocketEvent.write:
        // Socket is ready for writing
        break;
      case RawSocketEvent.readClosed:
        debugPrint('Socket read closed');
        _isConnected = false;
        break;
      case RawSocketEvent.closed:
        debugPrint('Socket closed');
        _isConnected = false;
        break;
    }
  }

  /// Wait for server response with timeout
  Future<bool> _waitForServerResponse() async {
    const timeout = Duration(seconds: 5);
    const checkInterval = Duration(milliseconds: 100);
    
    final stopwatch = Stopwatch()..start();
    
    while (stopwatch.elapsed < timeout) {
      if (_isConnected) {
        return true;
      }
      await Future.delayed(checkInterval);
    }
    
    return false;
  }

  /// Handle incoming data from server
  void _handleIncomingData() {
    if (_socket == null) return;

    final datagram = _socket!.receive();
    if (datagram != null) {
      // If we receive any data, we're connected
      if (!_isConnected) {
        _isConnected = true;
        debugPrint('Server response received - connection established');
      }
      
      // Process incoming frame data
      _processFrameData(datagram.data);
    }
  }

  /// Process incoming frame data.
  /// Fragmented protocol:
  /// [1 format][4 frame_id][8 timestamp_us][4 frame_size][2 chunk_idx][2 chunk_count][chunk_payload]
  /// Reassembles by frame_id and emits only complete, newer frames.
  void _processFrameData(Uint8List data) {
    if (data.length < 4) return;
    _cleanupOldAssemblies();

    // Fragmented protocol (preferred)
    if (data.length >= _kFragmentHeaderSize) {
      final frameSize = _parseFrameSizeBe(data, 13);
      final chunkIdx = _parseU16Be(data, 17);
      final chunkCount = _parseU16Be(data, 19);
      if (frameSize > 0 && chunkCount > 0 && chunkIdx < chunkCount) {
        final format = data[0];
        final frameId = _parseU32Be(data, 1);
        final timestampUs = _parseU64Be(data, 5);
        if (_isNewerFrameId(frameId)) {
          final chunkPayload = data.sublist(_kFragmentHeaderSize);
          final key = frameId & 0xFFFFFFFF;
          final assembly = _assemblies.putIfAbsent(
            key,
            () => _FrameAssembly(
              format: format,
              frameId: frameId,
              timestampUs: timestampUs,
              frameSize: frameSize,
              chunkCount: chunkCount,
            ),
          );

          // If metadata mismatches, restart assembly for this frame id.
          if (assembly.frameSize != frameSize || assembly.chunkCount != chunkCount || assembly.format != format) {
            _assemblies[key] = _FrameAssembly(
              format: format,
              frameId: frameId,
              timestampUs: timestampUs,
              frameSize: frameSize,
              chunkCount: chunkCount,
            );
          }
          _assemblies[key]!.addChunk(chunkIdx, chunkPayload);

          if (_assemblies[key]!.isComplete) {
            final payload = _assemblies[key]!.buildFrame();
            _assemblies.remove(key);
            if (payload != null && _isNewerFrameId(frameId)) {
              _lastEmittedFrameId = frameId & 0xFFFFFFFF;
              _frameController.add(VrFrameData(
                format: format,
                data: payload,
                frameId: frameId,
                timestampUs: timestampUs,
              ));
            }
          }
        }
        return;
      }
    }

    // New protocol with frame_id + timestamp: 17-byte header
    if (data.length >= _kFrameHeaderSize) {
      final sizeAt13 = _parseFrameSizeBe(data, 13);
      if (sizeAt13 > 0 && data.length >= _kFrameHeaderSize + sizeAt13) {
        final format = data[0];
        final frameId = _parseU32Be(data, 1);
        final timestampUs = _parseU64Be(data, 5);
        final payload = data.sublist(_kFrameHeaderSize, _kFrameHeaderSize + sizeAt13);
        if (_isNewerFrameId(frameId)) {
          _lastEmittedFrameId = frameId & 0xFFFFFFFF;
          _frameController.add(VrFrameData(
            format: format,
            data: payload,
            frameId: frameId,
            timestampUs: timestampUs,
          ));
        }
        return;
      }
    }

    // Legacy: [1 format][4 size BE][payload] — 5-byte header
    if (data.length >= 5) {
      final sizeLegacy = _parseFrameSizeBe(data, 1);
      if (sizeLegacy > 0 && data.length >= 5 + sizeLegacy) {
        final format = data[0];
        final payload = data.sublist(5, 5 + sizeLegacy);
        _frameController.add(VrFrameData(format: format, data: payload));
        return;
      }
    }

    // Legacy: [4 byte size BE][JPEG payload]
    final sizeLegacy = _parseFrameSizeBe(data, 0);
    if (sizeLegacy > 0 && data.length >= 4 + sizeLegacy) {
      final payload = data.sublist(4, 4 + sizeLegacy);
      _frameController.add(VrFrameData(format: VrFrameData.formatJpeg, data: payload));
    }
  }

  /// True if frameId is newer than _lastEmittedFrameId (handles 32-bit wrap).
  bool _isNewerFrameId(int frameId) {
    final last = _lastEmittedFrameId;
    if (last < 0) return true;
    final a = frameId & 0xFFFFFFFF;
    final b = last & 0xFFFFFFFF;
    final diff = (a - b) & 0xFFFFFFFF;
    return diff != 0 && diff < 0x80000000; // ahead in ring, not wrap-around
  }

  int _parseFrameSizeBe(Uint8List data, int offset) {
    if (data.length < offset + 4) return 0;
    return (data[offset] << 24) | (data[offset + 1] << 16) | (data[offset + 2] << 8) | data[offset + 3];
  }

  int _parseU32Be(Uint8List data, int offset) {
    if (data.length < offset + 4) return 0;
    return (data[offset] << 24) | (data[offset + 1] << 16) | (data[offset + 2] << 8) | data[offset + 3];
  }

  int _parseU64Be(Uint8List data, int offset) {
    if (data.length < offset + 8) return 0;
    final hi = _parseU32Be(data, offset);
    final lo = _parseU32Be(data, offset + 4);
    return (hi << 32) | (lo & 0xFFFFFFFF);
  }

  int _parseU16Be(Uint8List data, int offset) {
    if (data.length < offset + 2) return 0;
    return (data[offset] << 8) | data[offset + 1];
  }

  void _cleanupOldAssemblies() {
    if (_assemblies.isEmpty) return;
    final nowMs = DateTime.now().millisecondsSinceEpoch;
    final staleKeys = <int>[];
    _assemblies.forEach((key, assembly) {
      if (nowMs - assembly.createdMs > _kAssemblyTimeoutMs) {
        staleKeys.add(key);
      }
    });
    for (final key in staleKeys) {
      _assemblies.remove(key);
    }
  }

  /// Create connection message for server
  Uint8List _createConnectionMessage() {
    // Create a simple connection request message
    // This should match your C++ server's expected format
    final message = 'VR_CLIENT_CONNECT';
    return Uint8List.fromList(message.codeUnits);
  }

  /// Send control data to server (head tracking, etc.)
  void sendControlData(Map<String, dynamic> controlData) {
    if (!_isConnected || _socket == null) return;

    try {
      // Convert control data to bytes
      final message = _encodeControlData(controlData);
      final serverAddress = InternetAddress(_serverHost!);
      _socket!.send(message, serverAddress, _serverPort!);
    } catch (e) {
      debugPrint('Failed to send control data: $e');
    }
  }

  /// Send gyro as GYRO:yaw,pitch,roll (radians) for OpenEye server
  void sendGyro(double yaw, double pitch, double roll) {
    if (!_isConnected || _socket == null) return;

    try {
      final message = 'GYRO:$yaw,$pitch,$roll';
      final serverAddress = InternetAddress(_serverHost!);
      _socket!.send(Uint8List.fromList(message.codeUnits), serverAddress, _serverPort!);
    } catch (e) {
      debugPrint('Failed to send gyro: $e');
    }
  }

  /// Encode control data to bytes
  Uint8List _encodeControlData(Map<String, dynamic> data) {
    // Simple JSON-like encoding (adjust based on your server protocol)
    final jsonString = data.toString();
    return Uint8List.fromList(jsonString.codeUnits);
  }

  /// Discover VR servers on the network using UDP broadcast
  Future<void> discoverServers() async {
    try {
      debugPrint('🔍 Starting UDP broadcast server discovery...');
      
      // Create a discovery socket
      final discoverySocket = await RawDatagramSocket.bind(InternetAddress.anyIPv4, 0);
      
      // Enable broadcast on the socket
      discoverySocket.broadcastEnabled = true;
      
      // Send discovery broadcast to the hotspot network range
      final discoveryMessage = 'VR_SERVER_DISCOVERY_REQUEST';
      final hotspotBroadcast = InternetAddress('10.181.170.255'); // Hotspot broadcast address
      final discoveryPort = 27015; // Discovery port
      
      debugPrint('📡 Sending UDP broadcast to hotspot network: 10.181.170.255');
      discoverySocket.send(discoveryMessage.codeUnits, hotspotBroadcast, discoveryPort);
      
      // Also try general broadcast as fallback
      final generalBroadcast = InternetAddress('255.255.255.255');
      debugPrint('📡 Sending UDP broadcast to all networks: 255.255.255.255');
      discoverySocket.send(discoveryMessage.codeUnits, generalBroadcast, discoveryPort);
      
      // Listen for responses
      discoverySocket.listen((event) {
        if (event == RawSocketEvent.read) {
          final datagram = discoverySocket.receive();
          if (datagram != null) {
            final response = String.fromCharCodes(datagram.data);
            debugPrint('📨 Received response from ${datagram.address.address}: $response');
            
            if (response.startsWith('VR_SERVER_DISCOVERY_RESPONSE:')) {
              final serverInfo = response.substring('VR_SERVER_DISCOVERY_RESPONSE:'.length);
              final parts = serverInfo.split(':');
              
              if (parts.length >= 2) {
                final serverPort = parts[0];
                final serverName = parts[1];
                final serverAddress = datagram.address.address;
                
                // Create a more descriptive server entry
                final serverEntry = '$serverName ($serverAddress:$serverPort)';
                
                debugPrint('✅ Found VR server: $serverName');
                debugPrint('📡 Server address: $serverAddress:$serverPort');
                _discoveredServersController.add(serverEntry);
              }
            }
          }
        }
      });
      
      // Close discovery socket after 10 seconds
      Timer(const Duration(seconds: 10), () {
        discoverySocket.close();
        debugPrint('🔍 Discovery scan completed');
      });
      
    } catch (e) {
      debugPrint('❌ Server discovery failed: $e');
    }
  }

  /// Disconnect from server
  Future<void> disconnect() async {
    _isConnected = false;
    _lastEmittedFrameId = -1;
    _assemblies.clear();
    
    // Send disconnect message
    if (_socket != null && _serverHost != null && _serverPort != null) {
      try {
        final disconnectMessage = Uint8List.fromList('VR_CLIENT_DISCONNECT'.codeUnits);
        final serverAddress = InternetAddress(_serverHost!);
        _socket!.send(disconnectMessage, serverAddress, _serverPort!);
      } catch (e) {
        debugPrint('Error sending disconnect message: $e');
      }
    }
    
    // Close socket and subscription
    await _subscription?.cancel();
    _subscription = null;
    _socket?.close();
    _socket = null;
    
    // Close frame stream
    await _frameController.close();
    
    debugPrint('Disconnected from VR server');
  }

  /// Get connection info
  Map<String, dynamic> getConnectionInfo() {
    return {
      'host': _serverHost,
      'port': _serverPort,
      'connected': _isConnected,
    };
  }
}

class _FrameAssembly {
  _FrameAssembly({
    required this.format,
    required this.frameId,
    required this.timestampUs,
    required this.frameSize,
    required this.chunkCount,
  });

  final int format;
  final int frameId;
  final int timestampUs;
  final int frameSize;
  final int chunkCount;
  final int createdMs = DateTime.now().millisecondsSinceEpoch;
  final Map<int, Uint8List> _chunks = <int, Uint8List>{};
  int _receivedBytes = 0;

  void addChunk(int index, Uint8List chunk) {
    if (index < 0 || index >= chunkCount) return;
    if (_chunks.containsKey(index)) return;
    _chunks[index] = chunk;
    _receivedBytes += chunk.length;
  }

  bool get isComplete => _chunks.length == chunkCount && _receivedBytes >= frameSize;

  Uint8List? buildFrame() {
    if (!isComplete) return null;
    final bytesBuilder = BytesBuilder(copy: false);
    for (int i = 0; i < chunkCount; ++i) {
      final chunk = _chunks[i];
      if (chunk == null) return null;
      bytesBuilder.add(chunk);
    }
    final merged = bytesBuilder.takeBytes();
    if (merged.length < frameSize) return null;
    return merged.length == frameSize ? merged : Uint8List.sublistView(merged, 0, frameSize);
  }
}
