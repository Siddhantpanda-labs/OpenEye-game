import 'package:flutter/material.dart';
import 'package:flutter/foundation.dart';
import 'package:shared_preferences/shared_preferences.dart';
import '../networking/udp_client.dart';
import 'vr_screen.dart';
import 'qr_scan_screen.dart';

const _kLastServerHostKey = 'vr_last_server_host';
const _kLastServerPortKey = 'vr_last_server_port';
const _kDefaultPort = 27016;

class ConnectionScreen extends StatefulWidget {
  const ConnectionScreen({super.key});

  @override
  State<ConnectionScreen> createState() => _ConnectionScreenState();
}

class _ConnectionScreenState extends State<ConnectionScreen> {
  final _udpClient = UDPClient();
  final _hostController = TextEditingController();
  final _portController = TextEditingController(text: '$_kDefaultPort');

  bool _isConnecting = false;
  bool _isDiscovering = false;
  String _connectionStatus = 'Ready to connect';
  final List<String> _discoveredServers = [];

  @override
  void initState() {
    super.initState();
    _setupDiscoveryListener();
    _loadLastServer();
    WidgetsBinding.instance.addPostFrameCallback((_) => _tryReconnectToLastServer());
  }

  Future<void> _loadLastServer() async {
    final prefs = await SharedPreferences.getInstance();
    final host = prefs.getString(_kLastServerHostKey);
    final port = prefs.getInt(_kLastServerPortKey);
    if (host != null && host.isNotEmpty) {
      _hostController.text = host;
      if (port != null && port > 0 && port <= 65535) {
        _portController.text = '$port';
      }
    }
  }

  Future<void> _saveLastServer(String host, int port) async {
    final prefs = await SharedPreferences.getInstance();
    await prefs.setString(_kLastServerHostKey, host);
    await prefs.setInt(_kLastServerPortKey, port);
  }

  Future<void> _tryReconnectToLastServer() async {
    final prefs = await SharedPreferences.getInstance();
    final host = prefs.getString(_kLastServerHostKey);
    final port = prefs.getInt(_kLastServerPortKey) ?? _kDefaultPort;
    if (host == null || host.isEmpty || !mounted) return;
    setState(() {
      _isConnecting = true;
      _connectionStatus = 'Reconnecting to $host:$port...';
    });
    try {
      final success = await _udpClient.connect(host, port);
      if (!mounted) return;
      if (success) {
        setState(() => _connectionStatus = 'Connected!');
        Navigator.pushReplacement(
          context,
          MaterialPageRoute(builder: (context) => VRScreen(udpClient: _udpClient)),
        );
        return;
      }
    } catch (e) {
      if (kDebugMode) debugPrint('Reconnect failed: $e');
    }
    if (mounted) {
      setState(() {
        _isConnecting = false;
        _connectionStatus = 'Ready to connect';
      });
    }
  }

  void _setupDiscoveryListener() {
    _udpClient.discoveredServers.listen((serverInfo) {
      setState(() {
        if (!_discoveredServers.contains(serverInfo)) {
          _discoveredServers.add(serverInfo);
        }
      });
    });
  }


  Future<void> _discoverServers() async {
    setState(() {
      _isDiscovering = true;
      _discoveredServers.clear();
      _connectionStatus = 'Scanning for VR servers...';
    });

    await _udpClient.discoverServers();
    
    // Wait for discovery to complete
    await Future.delayed(const Duration(seconds: 6));
    
    setState(() {
      _isDiscovering = false;
      if (_discoveredServers.isEmpty) {
        _connectionStatus = 'No VR servers found. Check network connection.';
      } else {
        _connectionStatus = 'Found ${_discoveredServers.length} VR server(s)';
      }
    });
  }

  Future<void> _connectToDiscoveredServerEntry(String serverEntry) async {
    if (_isConnecting) return;

    setState(() {
      _isConnecting = true;
      _connectionStatus = 'Connecting to server...';
    });

    try {
      final success = await _udpClient.connectToDiscoveredServer(serverEntry);
      
      if (success) {
        setState(() => _connectionStatus = 'Connected successfully!');
        final match = RegExp(r'\((.+?):(\d+)\)').firstMatch(serverEntry);
        if (match != null) {
          await _saveLastServer(match.group(1)!, int.parse(match.group(2)!));
        }
        if (!mounted) return;
        Navigator.pushReplacement(
          context,
          MaterialPageRoute(builder: (context) => VRScreen(udpClient: _udpClient)),
        );
      } else {
        setState(() => _connectionStatus = 'Failed to connect to server');
      }
    } catch (e) {
      setState(() => _connectionStatus = 'Connection error: $e');
    } finally {
      if (mounted) setState(() => _isConnecting = false);
    }
  }

  Future<void> _connectToServer(String host, int port) async {
    if (_isConnecting) return;

    setState(() {
      _isConnecting = true;
      _connectionStatus = 'Connecting to $host:$port...';
    });

    try {
      final success = await _udpClient.connect(host, port);
      if (!mounted) return;
      if (success) {
        await _saveLastServer(host, port);
        setState(() => _connectionStatus = 'Connected!');
        Navigator.pushReplacement(
          context,
          MaterialPageRoute(builder: (context) => VRScreen(udpClient: _udpClient)),
        );
      } else {
        setState(() {
          _connectionStatus = 'Connection failed. Server may be offline.';
          _isConnecting = false;
        });
      }
    } catch (e) {
      if (mounted) {
        setState(() {
          _connectionStatus = 'Error: $e';
          _isConnecting = false;
        });
      }
    }
  }

  Future<void> _openQRScanner() async {
    if (_isConnecting) return;
    final result = await Navigator.push<VRServerScanResult>(
      context,
      MaterialPageRoute(builder: (context) => const QRScanScreen()),
    );
    if (result != null && mounted) {
      await _connectToServer(result.host, result.port);
    }
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      backgroundColor: Colors.black,
      body: SafeArea(
        child: Padding(
          padding: const EdgeInsets.all(20.0),
          child: Column(
            mainAxisAlignment: MainAxisAlignment.center,
            children: [
              // Header
              const Icon(
                Icons.view_in_ar,
                size: 80,
                color: Colors.blue,
              ),
              const SizedBox(height: 20),
              const Text(
                'VR Streaming Client',
                style: TextStyle(
                  fontSize: 28,
                  fontWeight: FontWeight.bold,
                  color: Colors.white,
                ),
              ),
              const SizedBox(height: 10),
              const Text(
                'Connect to Metaverse Game Server',
                style: TextStyle(
                  fontSize: 16,
                  color: Colors.grey,
                ),
              ),
              const SizedBox(height: 24),

              // Manual connect + QR (primary)
              Container(
                padding: const EdgeInsets.all(20),
                decoration: BoxDecoration(
                  color: Colors.grey[900],
                  borderRadius: BorderRadius.circular(12),
                ),
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.stretch,
                  children: [
                    const Text(
                      'Connect to server',
                      style: TextStyle(
                        color: Colors.white,
                        fontSize: 16,
                        fontWeight: FontWeight.bold,
                      ),
                    ),
                    const SizedBox(height: 12),
                    TextField(
                      controller: _hostController,
                      enabled: !_isConnecting,
                      decoration: InputDecoration(
                        labelText: 'Server IP',
                        hintText: '192.168.1.42',
                        filled: true,
                        fillColor: Colors.grey[800],
                        border: OutlineInputBorder(borderRadius: BorderRadius.circular(8)),
                      ),
                      style: const TextStyle(color: Colors.white),
                      keyboardType: TextInputType.numberWithOptions(decimal: true),
                    ),
                    const SizedBox(height: 10),
                    TextField(
                      controller: _portController,
                      enabled: !_isConnecting,
                      decoration: InputDecoration(
                        labelText: 'Port',
                        hintText: '$_kDefaultPort',
                        filled: true,
                        fillColor: Colors.grey[800],
                        border: OutlineInputBorder(borderRadius: BorderRadius.circular(8)),
                      ),
                      style: const TextStyle(color: Colors.white),
                      keyboardType: TextInputType.number,
                    ),
                    const SizedBox(height: 16),
                    Row(
                      children: [
                        Expanded(
                          child: ElevatedButton.icon(
                            onPressed: _isConnecting ? null : () {
                              final host = _hostController.text.trim();
                              final port = int.tryParse(_portController.text.trim()) ?? _kDefaultPort;
                              if (host.isEmpty) {
                                setState(() => _connectionStatus = 'Enter server IP');
                                return;
                              }
                              _connectToServer(host, port);
                            },
                            icon: const Icon(Icons.link),
                            label: const Text('Connect'),
                            style: ElevatedButton.styleFrom(
                              backgroundColor: Colors.blue,
                              foregroundColor: Colors.white,
                              padding: const EdgeInsets.symmetric(vertical: 14),
                            ),
                          ),
                        ),
                        const SizedBox(width: 12),
                        Expanded(
                          child: OutlinedButton.icon(
                            onPressed: _isConnecting ? null : _openQRScanner,
                            icon: const Icon(Icons.qr_code_scanner),
                            label: const Text('Scan QR'),
                            style: OutlinedButton.styleFrom(
                              foregroundColor: Colors.white,
                              side: const BorderSide(color: Colors.white54),
                              padding: const EdgeInsets.symmetric(vertical: 14),
                            ),
                          ),
                        ),
                      ],
                    ),
                  ],
                ),
              ),

              const SizedBox(height: 16),

              // Discovery (optional)
              Container(
                padding: const EdgeInsets.all(20),
                decoration: BoxDecoration(
                  color: Colors.grey[900],
                  borderRadius: BorderRadius.circular(12),
                ),
                child: Column(
                  children: [
                    SizedBox(
                      width: double.infinity,
                      height: 44,
                      child: OutlinedButton(
                        onPressed: _isDiscovering ? null : _discoverServers,
                        style: OutlinedButton.styleFrom(
                          foregroundColor: Colors.white70,
                          side: const BorderSide(color: Colors.white38),
                        ),
                        child: _isDiscovering
                            ? const SizedBox(
                                width: 20,
                                height: 20,
                                child: CircularProgressIndicator(strokeWidth: 2, color: Colors.white70),
                              )
                            : const Text('Scan network for servers'),
                      ),
                    ),
                    if (_discoveredServers.isNotEmpty) ...[
                      const SizedBox(height: 12),
                      ...(_discoveredServers.map((server) => ListTile(
                            title: Text(server, style: const TextStyle(color: Colors.white, fontSize: 14)),
                            trailing: const Icon(Icons.arrow_forward, color: Colors.blue, size: 20),
                            onTap: () => _connectToDiscoveredServerEntry(server),
                            dense: true,
                          ))),
                    ],
                  ],
                ),
              ),
              
              const SizedBox(height: 20),
              
              // Status
              Text(
                _connectionStatus,
                style: TextStyle(
                  color: _connectionStatus.contains('Connected') 
                      ? Colors.green 
                      : _connectionStatus.contains('Error') || _connectionStatus.contains('failed')
                          ? Colors.red
                          : Colors.grey,
                  fontSize: 14,
                ),
                textAlign: TextAlign.center,
              ),
              
              const SizedBox(height: 40),
              
              // Instructions
              Container(
                padding: const EdgeInsets.all(16),
                decoration: BoxDecoration(
                  color: Colors.blue.withValues(alpha: 0.1),
                  borderRadius: BorderRadius.circular(8),
                  border: Border.all(color: Colors.blue.withValues(alpha: 0.3)),
                ),
                child: const Column(
                  children: [
                    Icon(Icons.info_outline, color: Colors.blue),
                    SizedBox(height: 8),
                    Text(
                      'Enter server IP from the PC (or scan QR on server). Same Wi‑Fi required.',
                      style: TextStyle(color: Colors.grey),
                      textAlign: TextAlign.center,
                    ),
                  ],
                ),
              ),
            ],
          ),
        ),
      ),
    );
  }

  @override
  void dispose() {
    _hostController.dispose();
    _portController.dispose();
    super.dispose();
  }
}
