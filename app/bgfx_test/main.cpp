/**
 * BGFX test: exercises current BGFX/Vulkan setup.
 * - Prefers Vulkan first (then OpenGL, D3D11, Noop).
 * - When Vulkan is used, loads compiled copy_vs.bin/copy_fs.bin from
 *   exeDir/assets/shaders/bgfx/vulkan/ (or exeDir/shaders/bgfx/vulkan),
 *   draws a fullscreen quad with a 1x1 texture to verify shader load + draw.
 * Run from build/bin/Debug so assets/shaders/bgfx/vulkan/ resolves.
 */
#include <GLFW/glfw3.h>
#if defined(_WIN32)
#    define GLFW_EXPOSE_NATIVE_WIN32
#elif defined(__APPLE__)
#    define GLFW_EXPOSE_NATIVE_COCOA
#else
#    define GLFW_EXPOSE_NATIVE_X11
#endif
#include <GLFW/glfw3native.h>
#include <bgfx/bgfx.h>
#include <bgfx/platform.h>
#include <stdio.h>
#include <string>
#include <vector>

#if defined(_WIN32)
#    include <windows.h>
#endif

static std::string getExeDir() {
#if defined(_WIN32)
    char path[MAX_PATH];
    if (GetModuleFileNameA(NULL, path, MAX_PATH) == 0)
        return "";
    std::string s(path);
    size_t last = s.find_last_of("\\/");
    if (last != std::string::npos)
        s.resize(last);
    return s;
#else
    return "";  // Linux/OSX: could use /proc/self/exe or argv[0]
#endif
}

static bool readFile(const std::string& path, std::vector<uint8_t>& out) {
    out.clear();
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return false;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size <= 0) { fclose(f); return false; }
    out.resize(static_cast<size_t>(size));
    bool ok = fread(out.data(), 1, out.size(), f) == out.size();
    fclose(f);
    return ok;
}

static std::string findShaderBaseDir() {
    std::vector<uint8_t> dummy;
    auto tryBase = [&dummy](const std::string& base) -> bool {
        return readFile(base + "/copy_vs.bin", dummy);
    };
    std::string exeDir = getExeDir();
    if (!exeDir.empty()) {
        std::string a = exeDir + "/assets/shaders/bgfx/vulkan";
        if (tryBase(a)) return a;
        std::string b = exeDir + "/shaders/bgfx/vulkan";
        if (tryBase(b)) return b;
    }
    // Fallback: current working directory (e.g. when run from build/bin/Debug)
    if (tryBase("assets/shaders/bgfx/vulkan")) return "assets/shaders/bgfx/vulkan";
    if (tryBase("shaders/bgfx/vulkan")) return "shaders/bgfx/vulkan";
    return "";
}

int main() {
    if (!glfwInit()) {
        fprintf(stderr, "glfwInit failed\n");
        return 1;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(640, 480, "BGFX Vulkan/Shader Test", nullptr, nullptr);
    if (!window) {
        fprintf(stderr, "glfwCreateWindow failed\n");
        glfwTerminate();
        return 1;
    }
    glfwShowWindow(window);
    glfwPollEvents();

    bgfx::Init init = {};
    init.vendorId = BGFX_PCI_ID_NONE;
    init.deviceId = 0;
    init.resolution.width = 640;
    init.resolution.height = 480;
    init.resolution.reset = BGFX_RESET_VSYNC;

    // Set platform window handle so BGFX does not create a headless device.
    // Use _WIN32 so we always set nwh when building for Windows (BX_PLATFORM_* may not be set in this TU).
#if defined(_WIN32)
    init.platformData.ndt = nullptr;
    init.platformData.nwh = glfwGetWin32Window(window);
#elif defined(__linux__)
    init.platformData.ndt = glfwGetX11Display();
    init.platformData.nwh = (void*)(uintptr_t)glfwGetX11Window(window);
#elif defined(__APPLE__)
    init.platformData.nwh = glfwGetCocoaWindow(window);
#else
    init.platformData.ndt = nullptr;
    init.platformData.nwh = nullptr;
#endif
    init.platformData.context = nullptr;
    init.platformData.backBuffer = nullptr;
    init.platformData.backBufferDS = nullptr;

    // setPlatformData must be called before init (per bgfx API)
    bgfx::setPlatformData(init.platformData);

    // Prefer Vulkan first to test current Vulkan + compiled shader path
    bgfx::RendererType::Enum renderers[] = {
        bgfx::RendererType::Vulkan,
        bgfx::RendererType::OpenGL,
        bgfx::RendererType::Direct3D11,
        bgfx::RendererType::Noop,
    };
    const char* names[] = { "Vulkan", "OpenGL", "Direct3D11", "Noop" };
    bool inited = false;
    for (int i = 0; i < 4 && !inited; i++) {
        init.type = renderers[i];
        bgfx::setPlatformData(init.platformData);
        inited = bgfx::init(init);
    }
    if (!inited) {
        fprintf(stderr, "bgfx::init failed for all backends.\n");
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    const char* backendName = names[0];
    switch (bgfx::getRendererType()) {
        case bgfx::RendererType::Vulkan: backendName = "Vulkan"; break;
        case bgfx::RendererType::OpenGL: backendName = "OpenGL"; break;
        case bgfx::RendererType::Direct3D11: backendName = "Direct3D11"; break;
        default: backendName = "Other"; break;
    }
    printf("BGFX backend: %s\n", backendName);

    bool usedCopyShader = false;
    if (bgfx::getRendererType() == bgfx::RendererType::Vulkan) {
        std::string baseDir = findShaderBaseDir();
        if (!baseDir.empty()) {
            std::vector<uint8_t> vsData, fsData;
            if (readFile(baseDir + "/copy_vs.bin", vsData) && readFile(baseDir + "/copy_fs.bin", fsData)) {
                const bgfx::Memory* vsMem = bgfx::copy(vsData.data(), static_cast<uint32_t>(vsData.size()));
                const bgfx::Memory* fsMem = bgfx::copy(fsData.data(), static_cast<uint32_t>(fsData.size()));
                bgfx::ShaderHandle vsH = bgfx::createShader(vsMem);
                bgfx::ShaderHandle fsH = bgfx::createShader(fsMem);
                if (bgfx::isValid(vsH) && bgfx::isValid(fsH)) {
                    bgfx::ProgramHandle copyProg = bgfx::createProgram(vsH, fsH, false);
                    if (bgfx::isValid(copyProg)) {
                        // Fullscreen quad (position xy, texcoord xy)
                        const float quadVerts[] = {
                            -1.f, -1.f, 0.f, 1.f,
                             1.f, -1.f, 1.f, 1.f,
                            -1.f,  1.f, 0.f, 0.f,
                             1.f,  1.f, 1.f, 0.f,
                        };
                        const uint16_t quadIndices[] = { 0, 1, 2, 1, 3, 2 };
                        bgfx::VertexLayout layout;
                        layout.begin()
                            .add(bgfx::Attrib::Position, 2, bgfx::AttribType::Float)
                            .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
                            .end();
                        bgfx::VertexBufferHandle vb = bgfx::createVertexBuffer(bgfx::copy(quadVerts, sizeof(quadVerts)), layout);
                        bgfx::IndexBufferHandle ib = bgfx::createIndexBuffer(bgfx::copy(quadIndices, sizeof(quadIndices)));
                        // 1x1 magenta texture so we see something different from clear
                        uint32_t pixel = 0xFF00FFFF;  // RGBA
                        bgfx::TextureHandle tex = bgfx::createTexture2D(1, 1, false, 1, bgfx::TextureFormat::RGBA8, 0, bgfx::copy(&pixel, 4));
                        bgfx::UniformHandle uTexture = bgfx::createUniform("u_Texture", bgfx::UniformType::Sampler);

                        if (bgfx::isValid(vb) && bgfx::isValid(ib) && bgfx::isValid(tex) && bgfx::isValid(uTexture)) {
                            bgfx::setViewClear(0, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x223355FF, 1.0f, 0);
                            bgfx::setViewRect(0, 0, 0, 640, 480);
                            bgfx::setVertexBuffer(0, vb);
                            bgfx::setIndexBuffer(ib);
                            bgfx::setTexture(0, uTexture, tex);
                            bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
                            bgfx::submit(0, copyProg);
                            usedCopyShader = true;
                            printf("Drew fullscreen quad with copy shader (Vulkan .bin)\n");
                        }

                        if (bgfx::isValid(uTexture)) bgfx::destroy(uTexture);
                        if (bgfx::isValid(tex)) bgfx::destroy(tex);
                        if (bgfx::isValid(ib)) bgfx::destroy(ib);
                        if (bgfx::isValid(vb)) bgfx::destroy(vb);
                        bgfx::destroy(copyProg);
                    }
                }
                if (bgfx::isValid(fsH)) bgfx::destroy(fsH);
                if (bgfx::isValid(vsH)) bgfx::destroy(vsH);
            }
        }
        if (!usedCopyShader)
            fprintf(stderr, "Vulkan copy shaders not found. Run from build/bin/Debug and run compile_bgfx_shaders.py.\n");
    }

    if (!usedCopyShader) {
        bgfx::setViewClear(0, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x443355FF, 1.0f, 0);
        bgfx::setViewRect(0, 0, 0, 640, 480);
        bgfx::touch(0);
    }

    bgfx::frame();

    bgfx::shutdown();
    glfwDestroyWindow(window);
    glfwTerminate();
    printf("BGFX test OK: %s -> one frame -> shutdown\n", usedCopyShader ? "Vulkan+copy shader" : backendName);
    return 0;
}
