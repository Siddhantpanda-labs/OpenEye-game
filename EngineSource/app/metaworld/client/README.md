# 🎮 VR Streaming Client

Mobile VR client for the Metaverse Game streaming system built with Flutter.

## 🚀 Quick Start

### Prerequisites
- **Flutter SDK** (3.10.0+)
- **Android Studio** or **VS Code**
- **Android device** with gyroscope (API 21+)
- **VR headset** (Cardboard, etc.)

### Setup
```bash
# Windows
cd app/metaworld/client
setup.bat

# Linux/macOS  
cd app/metaworld/client
chmod +x setup.sh
./setup.sh
```

### Run
```bash
cd flutter_app
flutter run
```

## 📱 Features

### ✅ Implemented
- **Cross-platform** (Android + iOS)
- **UDP networking** with GameNetworkingSockets protocol
- **Head tracking** with gyroscope/accelerometer
- **Stereo rendering** for left/right eye
- **Real-time streaming** from C++ server
- **VR controls** and calibration
- **Connection management** with saved settings

### 🎯 VR Experience
- **Cardboard VR** support
- **Head tracking** for looking around
- **Stereo rendering** for depth perception
- **Touch controls** for interaction
- **Calibration** for accurate tracking

## 🏗️ Architecture

### Project Structure
```
client/
├── flutter_app/              # Main Flutter app
│   ├── lib/
│   │   ├── main.dart         # App entry point
│   │   ├── ui/               # User interface
│   │   │   ├── connection_screen.dart
│   │   │   └── vr_screen.dart
│   │   ├── vr/               # VR functionality
│   │   │   ├── vr_renderer.dart
│   │   │   ├── vr_controls.dart
│   │   │   └── stereo_renderer.dart
│   │   └── networking/       # Network communication
│   │       └── udp_client.dart
│   ├── android/              # Android configuration
│   └── pubspec.yaml          # Dependencies
├── setup.bat                 # Windows setup script
├── setup.sh                  # Unix setup script
└── setup.md                  # Detailed setup guide
```

### Key Components

#### 1. **UDP Client** (`networking/udp_client.dart`)
- Connects to C++ VR streaming server
- Handles frame data reception
- Sends head tracking data back to server
- Manages connection lifecycle

#### 2. **VR Renderer** (`vr/vr_renderer.dart`)
- Renders received frames in VR
- Handles stereo rendering for left/right eye
- Applies head tracking transformations
- Manages VR distortion correction

#### 3. **VR Controls** (`vr/vr_controls.dart`)
- Gyroscope/accelerometer integration
- Head tracking data processing
- Touch input handling
- Calibration system

#### 4. **Connection Screen** (`ui/connection_screen.dart`)
- Server connection interface
- Settings management
- Connection status display
- Error handling

## 🌐 Network Protocol

### Connection Flow
1. **Client connects** to server via UDP
2. **Server streams** rendered frames
3. **Client sends** head tracking data
4. **Real-time** bidirectional communication

### Data Format
- **Frames:** RGBA 1280x720 pixels
- **Head Tracking:** Yaw, Pitch, Roll angles
- **Protocol:** UDP with GameNetworkingSockets

## 📊 Performance

### Target Specifications
- **Frame Rate:** 30 FPS
- **Resolution:** 1280x720
- **Latency:** < 50ms
- **Bandwidth:** ~3.7MB/s

### Optimization
- **Release builds** for best performance
- **Background app** management
- **Network quality** monitoring
- **Adaptive quality** based on conditions

## 🔧 Configuration

### Server Settings
- **Default Host:** 192.168.1.100
- **Default Port:** 27015
- **Protocol:** UDP
- **Settings saved** automatically

### VR Settings
- **IPD:** 64mm (configurable)
- **FOV:** 90° (configurable)
- **Calibration:** Manual reset
- **Orientation:** Landscape only

## 🎮 Usage

### 1. **Connect to Server**
- Launch app
- Enter server IP address
- Tap "Connect to VR Server"

### 2. **VR Setup**
- Place phone in VR headset
- Tap "Calibrate" to reset tracking
- Move head to look around

### 3. **VR Experience**
- Look around with head movement
- Touch screen for interaction
- Tap red X to exit VR

## 🐛 Troubleshooting

### Common Issues

#### Connection Failed
- ✅ Check server IP and port
- ✅ Ensure server is running
- ✅ Check firewall settings
- ✅ Verify network connectivity

#### Head Tracking Issues
- ✅ Grant sensor permissions
- ✅ Ensure device has gyroscope
- ✅ Try recalibrating
- ✅ Check device orientation

#### Performance Issues
- ✅ Use release build
- ✅ Close background apps
- ✅ Check network quality
- ✅ Reduce server quality if needed

## 🚀 Next Steps

### Planned Features
- **H.264/H.265 encoding** for better compression
- **Adaptive bitrate** based on network
- **Hand tracking** support
- **Controller integration**
- **Spatial audio** support

### Development
- **OpenGL ES** integration for better rendering
- **Native plugins** for performance
- **Advanced VR features**
- **Multi-user support**

## 📞 Support

### Getting Help
1. **Check setup.md** for detailed instructions
2. **Review Flutter docs** for framework issues
3. **Check server logs** for connection problems
4. **Test network** connectivity

### Development
- **Flutter version:** 3.10.0+
- **Dart version:** 3.0.0+
- **Android API:** 21+
- **iOS version:** 11.0+

---

**🎯 Ready to experience VR streaming! Connect your phone to the server and enjoy immersive VR gaming!**