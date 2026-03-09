// Windows VR stream test client: same protocol as Flutter.
// Discovery -> connect -> receive frames (JPEG/H.264), decode, display in GLFW window.
// Mouse drag sends GYRO (yaw, pitch) like Flutter client.

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

#include "FrameDecoder.h"
#include "VRStreamClient.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <mutex>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <combaseapi.h>
#endif

namespace {

struct DisplayState {
  std::mutex mutex;
  std::vector<uint8_t> rgb;
  int width{0};
  int height{0};
  bool updated{false};
};

void recvThread(winclient::VRStreamClient* client, winclient::FrameDecoder* decoder,
                DisplayState* display, std::atomic<bool>* running) {
#ifdef _WIN32
  char buf[2048];
  struct sockaddr_in from{};
  SOCKET s = (SOCKET)(intptr_t)client->getSocketForRecv();
  if (s == INVALID_SOCKET || s == 0) return;

  static std::atomic<int> framesDecoded{0};
  int packetsReceived = 0;
  auto t0 = std::chrono::steady_clock::now();
  bool loggedNoPackets = false;

  while (*running) {
    int fromLen = sizeof(from);
    int n = recvfrom(s, buf, sizeof(buf), 0, (struct sockaddr*)&from, &fromLen);
    if (n > 0) {
      packetsReceived++;
      if (packetsReceived <= 5) {
        std::printf("[recv] packet #%d size=%d (header byte=%d)\n", packetsReceived, n, n >= 1 ? (int)(unsigned char)buf[0] : -1);
        std::fflush(stdout);
      } else if (packetsReceived == 6) {
        std::printf("[recv] ... (packets continuing)\n");
        std::fflush(stdout);
      }

      if (client->feedDatagram((const uint8_t*)buf, (size_t)n)) {
        std::printf("[recv] complete frame assembled\n");
        std::fflush(stdout);
        winclient::Frame frame;
        if (client->takeLatestFrame(frame) && !frame.data.empty()) {
          std::vector<uint8_t> outRgb;
          int w = 0, h = 0;
          bool needMoreInput = false;
          if (decoder->decode(frame.format, frame.data.data(), frame.data.size(), outRgb, w, h, &needMoreInput)) {
            if (framesDecoded++ == 0) {
              std::printf("First frame decoded %dx%d (format %d)\n", w, h, (int)frame.format);
              std::fflush(stdout);
            }
            std::lock_guard<std::mutex> lock(display->mutex);
            display->rgb = std::move(outRgb);
            display->width = w;
            display->height = h;
            display->updated = true;
          } else if (!needMoreInput) {
            std::printf("[recv] decode failed format=%d size=%zu\n", (int)frame.format, frame.data.size());
            std::fflush(stdout);
          }
        }
      }
    } else {
      DWORD err = WSAGetLastError();
      if (err == WSAEWOULDBLOCK || err == WSAETIMEDOUT) {
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - t0).count();
        if (elapsed >= 2 && packetsReceived == 0 && !loggedNoPackets) {
          loggedNoPackets = true;
          std::printf("[recv] No packets in 2s - is server sending frames? (Check VR stream is active on server.)\n");
          std::fflush(stdout);
        }
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
  }
#else
  (void)client;
  (void)decoder;
  (void)display;
  (void)running;
#endif
}

} // namespace

int main(int argc, char** argv) {
#ifdef _WIN32
  if (FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED)))
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
#endif

  std::string host = "127.0.0.1";
  uint16_t port = 27016;

  if (argc >= 2) host = argv[1];
  if (argc >= 3) port = (uint16_t)atoi(argv[2]);

  winclient::VRStreamClient client;
  if (argc <= 1) {
    std::string name;
    printf("Discovering VR server...\n");
    if (client.discover(host, port, name, 5000)) {
      printf("Found: %s at %s:%u\n", name.c_str(), host.c_str(), (unsigned)port);
    } else {
      printf("No server found. Using %s:%u\n", host.c_str(), (unsigned)port);
    }
  }

  if (!client.connect(host, port)) {
    fprintf(stderr, "Connect failed to %s:%u\n", host.c_str(), (unsigned)port);
    return 1;
  }
#ifdef _WIN32
  SOCKET s = (SOCKET)(intptr_t)client.getSocketForRecv();
  if (s != INVALID_SOCKET) {
    unsigned long nonBlock = 1;
    ioctlsocket(s, FIONBIO, &nonBlock);
  }
#endif
  printf("Connected. Drag mouse to send head rotation. Close window to quit.\n");

  if (!glfwInit()) {
    fprintf(stderr, "glfwInit failed\n");
    return 1;
  }

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  GLFWwindow* window = glfwCreateWindow(960, 540, "VR Stream Client", nullptr, nullptr);
  if (!window) {
    glfwTerminate();
    return 1;
  }
  glfwMakeContextCurrent(window);
  if (!gladLoadGL(reinterpret_cast<GLADloadfunc>(glfwGetProcAddress))) {
    glfwDestroyWindow(window);
    glfwTerminate();
    return 1;
  }

  DisplayState display;
  winclient::FrameDecoder decoder;
  std::atomic<bool> running{true};

  std::thread recvTh(recvThread, &client, &decoder, &display, &running);

  GLuint tex = 0;
  glGenTextures(1, &tex);
  glBindTexture(GL_TEXTURE_2D, tex);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  // Initialize so quad draws something before first frame
  unsigned char initPixel[3] = { 40, 40, 40 };
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, initPixel);

  // Fullscreen quad: 6 vertices (2 triangles)
  const char* vs = R"(
#version 330 core
out vec2 uv;
void main() {
  int v = gl_VertexID;
  float x = float(v == 1 || v == 3 || v == 4) * 2.0 - 1.0;
  float y = float(v >= 2) * 2.0 - 1.0;
  gl_Position = vec4(x, y, 0.0, 1.0);
  uv = vec2(float(v == 1 || v == 3 || v == 4), float(v >= 2));
}
)";
  const char* fs = R"(
#version 330 core
uniform sampler2D uTex;
in vec2 uv;
out vec4 fragColor;
void main() {
  fragColor = texture(uTex, uv);
}
)";

  GLuint prog = 0;
  GLuint vsh = glCreateShader(GL_VERTEX_SHADER);
  glShaderSource(vsh, 1, &vs, nullptr);
  glCompileShader(vsh);
  GLuint fsh = glCreateShader(GL_FRAGMENT_SHADER);
  glShaderSource(fsh, 1, &fs, nullptr);
  glCompileShader(fsh);
  prog = glCreateProgram();
  glAttachShader(prog, vsh);
  glAttachShader(prog, fsh);
  glLinkProgram(prog);
  glDeleteShader(vsh);
  glDeleteShader(fsh);

  double mouseYaw = 0.0, mousePitch = 0.0;
  double lastX = 0.0, lastY = 0.0;
  bool firstMouse = true;
  const double gyroScale = 0.002;

  auto lastGyroSend = std::chrono::steady_clock::now();

  while (!glfwWindowShouldClose(window)) {
    glfwPollEvents();

    double x, y;
    glfwGetCursorPos(window, &x, &y);
    if (firstMouse) {
      lastX = x;
      lastY = y;
      firstMouse = false;
    }
    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
      double dx = x - lastX;
      double dy = y - lastY;
      mouseYaw += dx * gyroScale;
      mousePitch -= dy * gyroScale;
      if (mousePitch > 1.57) mousePitch = 1.57;
      if (mousePitch < -1.57) mousePitch = -1.57;
    }
    lastX = x;
    lastY = y;

    auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastGyroSend).count() >= 16) {
      client.sendGyro(mouseYaw, mousePitch, 0.0);
      lastGyroSend = now;
    }

    {
      std::lock_guard<std::mutex> lock(display.mutex);
      if (display.updated && display.width > 0 && display.height > 0 && !display.rgb.empty()) {
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, display.width, display.height, 0,
                     GL_RGB, GL_UNSIGNED_BYTE, display.rgb.data());
        display.updated = false;
      }
    }

    int fbW, fbH;
    glfwGetFramebufferSize(window, &fbW, &fbH);
    glViewport(0, 0, fbW, fbH);
    glClearColor(0.1f, 0.1f, 0.1f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(prog);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex);
    glUniform1i(glGetUniformLocation(prog, "uTex"), 0);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    glfwSwapBuffers(window);
  }

  running = false;
  if (recvTh.joinable()) recvTh.join();

  client.sendDisconnect();
  client.disconnect();

  glDeleteProgram(prog);
  glDeleteTextures(1, &tex);
  glfwDestroyWindow(window);
  glfwTerminate();

#ifdef _WIN32
  CoUninitialize();
#endif
  return 0;
}
