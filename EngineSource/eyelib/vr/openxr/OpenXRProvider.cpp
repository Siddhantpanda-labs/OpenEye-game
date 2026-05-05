#include "eyelib/vr/openxr/OpenXRProvider.h"
#include "eyelib/vr/XRTypes.h"
#include "eyelib/math/Types.h"

#include <glad/glad.h>

#if defined(_WIN32)
#  define XR_USE_PLATFORM_WIN32
#  include <windows.h>
#endif
#define XR_USE_GRAPHICS_API_OPENGL
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

#include <cstring>
#include <vector>

namespace eyelib {
namespace vr {

struct OpenXRProvider::Impl {
    XrInstance instance = XR_NULL_HANDLE;
    XrSystemId systemId = XR_NULL_SYSTEM_ID;
    XrSession session = XR_NULL_HANDLE;
    XrSpace referenceSpace = XR_NULL_HANDLE;
    XrViewConfigurationType viewConfigType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
    std::vector<XrViewConfigurationView> viewConfigViews;
    std::vector<XrSwapchain> swapchains;
    std::vector<std::vector<XrSwapchainImageOpenGLKHR>> swapchainImages;
    uint32_t viewCount = 0;
    XrTime displayTime = 0;
    std::vector<XrView> views;
    std::vector<XrCompositionLayerProjectionView> projectionViews;
    bool frameBegun = false;

    ~Impl() {
        if (session != XR_NULL_HANDLE) {
            xrEndSession(session);
            for (XrSwapchain sc : swapchains) {
                if (sc != XR_NULL_HANDLE) xrDestroySwapchain(sc);
            }
            swapchains.clear();
            swapchainImages.clear();
            if (referenceSpace != XR_NULL_HANDLE) xrDestroySpace(referenceSpace);
            xrDestroySession(session);
            session = XR_NULL_HANDLE;
        }
        if (instance != XR_NULL_HANDLE) {
            xrDestroyInstance(instance);
            instance = XR_NULL_HANDLE;
        }
    }
};

OpenXRProvider::OpenXRProvider() : m_impl(std::make_unique<Impl>()) {}

OpenXRProvider::~OpenXRProvider() = default;

bool OpenXRProvider::Init() {
    if (m_impl->instance != XR_NULL_HANDLE) return true;

    const char* const extensions[] = {
        "XR_KHR_opengl_enable",
        "XR_KHR_win32_enable",
    };
    XrInstanceCreateInfo createInfo{};
    createInfo.type = XR_TYPE_INSTANCE_CREATE_INFO;
    createInfo.enabledExtensionCount = sizeof(extensions) / sizeof(extensions[0]);
    createInfo.enabledExtensionNames = extensions;
    strncpy(createInfo.applicationInfo.applicationName, "OpenEye", XR_MAX_APPLICATION_NAME_SIZE - 1);
    createInfo.applicationInfo.applicationVersion = 1;
    strncpy(createInfo.applicationInfo.engineName, "OpenEye", XR_MAX_ENGINE_NAME_SIZE - 1);
    createInfo.applicationInfo.engineVersion = 1;
    createInfo.applicationInfo.apiVersion = XR_CURRENT_API_VERSION;

    XrResult res = xrCreateInstance(&createInfo, &m_impl->instance);
    if (res != XR_SUCCESS || m_impl->instance == XR_NULL_HANDLE) return false;

    XrSystemGetInfo systemInfo{};
    systemInfo.type = XR_TYPE_SYSTEM_GET_INFO;
    systemInfo.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
    res = xrGetSystem(m_impl->instance, &systemInfo, &m_impl->systemId);
    if (res != XR_SUCCESS || m_impl->systemId == XR_NULL_SYSTEM_ID) return false;

    return true;
}

void OpenXRProvider::Shutdown() {
    DestroySession();
    if (m_impl->instance != XR_NULL_HANDLE) {
        xrDestroyInstance(m_impl->instance);
        m_impl->instance = XR_NULL_HANDLE;
    }
}

bool OpenXRProvider::CreateSession(void* graphicsBinding) {
    if (m_impl->session != XR_NULL_HANDLE) return true;
    if (m_impl->instance == XR_NULL_HANDLE || !Init()) return false;

    const XrGraphicsBindingOpenGLWin32KHR* glBinding = static_cast<const XrGraphicsBindingOpenGLWin32KHR*>(graphicsBinding);
    if (!glBinding || glBinding->type != XR_TYPE_GRAPHICS_BINDING_OPENGL_WIN32_KHR) return false;

    XrSessionCreateInfo sessionInfo{};
    sessionInfo.type = XR_TYPE_SESSION_CREATE_INFO;
    sessionInfo.next = glBinding;
    sessionInfo.systemId = m_impl->systemId;

    XrResult res = xrCreateSession(m_impl->instance, &sessionInfo, &m_impl->session);
    if (res != XR_SUCCESS || m_impl->session == XR_NULL_HANDLE) return false;

    XrReferenceSpaceCreateInfo spaceInfo{};
    spaceInfo.type = XR_TYPE_REFERENCE_SPACE_CREATE_INFO;
    spaceInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
    spaceInfo.poseInReferenceSpace = {{0, 0, 0, 1}, {0, 0, 0}};
    res = xrCreateReferenceSpace(m_impl->session, &spaceInfo, &m_impl->referenceSpace);
    if (res != XR_SUCCESS) return false;

    uint32_t viewCount = 0;
    res = xrEnumerateViewConfigurationViews(m_impl->instance, m_impl->systemId,
                                            m_impl->viewConfigType, 0, &viewCount, nullptr);
    if (res != XR_SUCCESS || viewCount == 0) return false;

    m_impl->viewConfigViews.resize(viewCount, {XR_TYPE_VIEW_CONFIGURATION_VIEW});
    res = xrEnumerateViewConfigurationViews(m_impl->instance, m_impl->systemId,
                                            m_impl->viewConfigType, viewCount, &viewCount,
                                            m_impl->viewConfigViews.data());
    if (res != XR_SUCCESS) return false;

    m_impl->viewCount = viewCount;
    m_impl->views.resize(viewCount, {XR_TYPE_VIEW});
    m_impl->projectionViews.resize(viewCount, {XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW});

    uint32_t formatCount = 0;
    xrEnumerateSwapchainFormats(m_impl->session, 0, &formatCount, nullptr);
    std::vector<int64_t> formats(formatCount);
    xrEnumerateSwapchainFormats(m_impl->session, formatCount, &formatCount, formats.data());
    int64_t chosenFormat = 0;
    for (int64_t f : formats) {
        if (f == (int64_t)0x8C43 || f == (int64_t)0x8058) { chosenFormat = f; break; }  // GL_SRGB8_ALPHA8, GL_RGBA8
    }
    if (chosenFormat == 0 && !formats.empty()) chosenFormat = formats[0];

    m_impl->swapchains.clear();
    m_impl->swapchainImages.clear();
    for (uint32_t i = 0; i < viewCount; ++i) {
        const auto& vcv = m_impl->viewConfigViews[i];
        XrSwapchainCreateInfo swapInfo{};
        swapInfo.type = XR_TYPE_SWAPCHAIN_CREATE_INFO;
        swapInfo.arraySize = 1;
        swapInfo.format = chosenFormat;
        swapInfo.width = vcv.recommendedImageRectWidth;
        swapInfo.height = vcv.recommendedImageRectHeight;
        swapInfo.mipCount = 1;
        swapInfo.sampleCount = vcv.recommendedSwapchainSampleCount;
        swapInfo.faceCount = 1;
        swapInfo.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;

        XrSwapchain sc = XR_NULL_HANDLE;
        res = xrCreateSwapchain(m_impl->session, &swapInfo, &sc);
        if (res != XR_SUCCESS) return false;
        m_impl->swapchains.push_back(sc);

        uint32_t imageCount = 0;
        xrEnumerateSwapchainImages(sc, 0, &imageCount, nullptr);
        std::vector<XrSwapchainImageOpenGLKHR> images(imageCount, {XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_KHR});
        xrEnumerateSwapchainImages(sc, imageCount, &imageCount, (XrSwapchainImageBaseHeader*)images.data());
        m_impl->swapchainImages.push_back(std::move(images));
    }

    XrSessionBeginInfo beginInfo{};
    beginInfo.type = XR_TYPE_SESSION_BEGIN_INFO;
    beginInfo.primaryViewConfigurationType = m_impl->viewConfigType;
    res = xrBeginSession(m_impl->session, &beginInfo);
    if (res != XR_SUCCESS) return false;

    return true;
}

void OpenXRProvider::DestroySession() {
    if (m_impl->session == XR_NULL_HANDLE) return;
    xrEndSession(m_impl->session);
    for (XrSwapchain sc : m_impl->swapchains) {
        if (sc != XR_NULL_HANDLE) xrDestroySwapchain(sc);
    }
    m_impl->swapchains.clear();
    m_impl->swapchainImages.clear();
    if (m_impl->referenceSpace != XR_NULL_HANDLE) {
        xrDestroySpace(m_impl->referenceSpace);
        m_impl->referenceSpace = XR_NULL_HANDLE;
    }
    xrDestroySession(m_impl->session);
    m_impl->session = XR_NULL_HANDLE;
    m_impl->viewCount = 0;
    m_impl->frameBegun = false;
}

bool OpenXRProvider::IsSessionActive() const {
    return m_impl->session != XR_NULL_HANDLE;
}

uint32_t OpenXRProvider::GetViewCount() const {
    return m_impl->viewCount;
}

bool OpenXRProvider::GetViewPoseFov(uint32_t viewIndex, XRViewData& out) const {
    if (viewIndex >= m_impl->views.size() || !m_impl->frameBegun) return false;
    const XrView& v = m_impl->views[viewIndex];
    out.position.x = v.pose.position.x;
    out.position.y = v.pose.position.y;
    out.position.z = v.pose.position.z;
    out.orientation.w = v.pose.orientation.w;
    out.orientation.x = v.pose.orientation.x;
    out.orientation.y = v.pose.orientation.y;
    out.orientation.z = v.pose.orientation.z;
    out.angleLeft = v.fov.angleLeft;
    out.angleRight = v.fov.angleRight;
    out.angleUp = v.fov.angleUp;
    out.angleDown = v.fov.angleDown;
    return true;
}

void OpenXRProvider::GetRecommendedViewSize(uint32_t viewIndex, uint32_t& width, uint32_t& height) const {
    width = height = 0;
    if (viewIndex < m_impl->viewConfigViews.size()) {
        width = m_impl->viewConfigViews[viewIndex].recommendedImageRectWidth;
        height = m_impl->viewConfigViews[viewIndex].recommendedImageRectHeight;
    }
}

bool OpenXRProvider::BeginFrame() {
    if (m_impl->session == XR_NULL_HANDLE) return false;

    XrFrameWaitInfo waitInfo{};
    waitInfo.type = XR_TYPE_FRAME_WAIT_INFO;
    XrFrameState frameState{};
    frameState.type = XR_TYPE_FRAME_STATE;
    XrResult res = xrWaitFrame(m_impl->session, &waitInfo, &frameState);
    if (res != XR_SUCCESS) return false;
    m_impl->displayTime = frameState.predictedDisplayTime;

    XrFrameBeginInfo beginInfo{};
    beginInfo.type = XR_TYPE_FRAME_BEGIN_INFO;
    res = xrBeginFrame(m_impl->session, &beginInfo);
    if (res != XR_SUCCESS) return false;
    m_impl->frameBegun = true;

    XrViewLocateInfo locateInfo{};
    locateInfo.type = XR_TYPE_VIEW_LOCATE_INFO;
    locateInfo.viewConfigurationType = m_impl->viewConfigType;
    locateInfo.displayTime = m_impl->displayTime;
    locateInfo.space = m_impl->referenceSpace;
    XrViewState viewState{};
    viewState.type = XR_TYPE_VIEW_STATE;
    uint32_t count = static_cast<uint32_t>(m_impl->views.size());
    res = xrLocateViews(m_impl->session, &locateInfo, &viewState, count, &count, m_impl->views.data());
    if (res != XR_SUCCESS) return false;

    return true;
}

bool OpenXRProvider::EndFrame(uint32_t viewCount, const XRTextureHandle* textureHandles) {
    if (!m_impl->frameBegun || m_impl->session == XR_NULL_HANDLE) return false;

    std::vector<uint32_t> acquiredIndex(static_cast<size_t>(viewCount), 0);
    for (uint32_t i = 0; i < viewCount && i < m_impl->swapchains.size(); ++i) {
        XrSwapchainImageAcquireInfo acquireInfo{};
        acquireInfo.type = XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO;
        XrResult ar = xrAcquireSwapchainImage(m_impl->swapchains[i], &acquireInfo, &acquiredIndex[i]);
        if (ar != XR_SUCCESS) {
            m_impl->frameBegun = false;
            return false;
        }
    }

    XrSwapchainImageWaitInfo waitInfo{};
    waitInfo.type = XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO;
    waitInfo.timeout = XR_INFINITE_DURATION;
    for (uint32_t i = 0; i < viewCount && i < m_impl->swapchains.size(); ++i) {
        xrWaitSwapchainImage(m_impl->swapchains[i], &waitInfo);
    }

    if (textureHandles && viewCount <= m_impl->swapchainImages.size()) {
        for (uint32_t i = 0; i < viewCount; ++i) {
            size_t idx = static_cast<size_t>(acquiredIndex[i]);
            if (idx >= m_impl->swapchainImages[i].size()) continue;
            GLuint dstTexture = m_impl->swapchainImages[i][idx].image;
            GLuint srcTexture = static_cast<GLuint>(textureHandles[i]);
            if (srcTexture != 0 && dstTexture != 0) {
                uint32_t w = m_impl->viewConfigViews[i].recommendedImageRectWidth;
                uint32_t h = m_impl->viewConfigViews[i].recommendedImageRectHeight;
                glCopyImageSubData(srcTexture, GL_TEXTURE_2D, 0, 0, 0, 0,
                                  dstTexture, GL_TEXTURE_2D, 0, 0, 0, 0,
                                  static_cast<GLsizei>(w), static_cast<GLsizei>(h), 1);
            }
        }
    }

    for (uint32_t i = 0; i < viewCount && i < m_impl->swapchains.size(); ++i) {
        XrSwapchainImageReleaseInfo releaseInfo{};
        releaseInfo.type = XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO;
        xrReleaseSwapchainImage(m_impl->swapchains[i], &releaseInfo);
    }

    for (uint32_t i = 0; i < viewCount && i < m_impl->projectionViews.size(); ++i) {
        m_impl->projectionViews[i].type = XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW;
        m_impl->projectionViews[i].pose = m_impl->views[i].pose;
        m_impl->projectionViews[i].fov = m_impl->views[i].fov;
        m_impl->projectionViews[i].subImage.swapchain = m_impl->swapchains[i];
        m_impl->projectionViews[i].subImage.imageRect.offset = {0, 0};
        m_impl->projectionViews[i].subImage.imageRect.extent.width = m_impl->viewConfigViews[i].recommendedImageRectWidth;
        m_impl->projectionViews[i].subImage.imageRect.extent.height = m_impl->viewConfigViews[i].recommendedImageRectHeight;
        m_impl->projectionViews[i].subImage.imageArrayIndex = 0;
    }

    XrCompositionLayerProjection layer{};
    layer.type = XR_TYPE_COMPOSITION_LAYER_PROJECTION;
    layer.space = m_impl->referenceSpace;
    layer.viewCount = viewCount;
    layer.views = m_impl->projectionViews.data();

    XrCompositionLayerBaseHeader* layers[] = { (XrCompositionLayerBaseHeader*)&layer };
    XrFrameEndInfo endInfo{};
    endInfo.type = XR_TYPE_FRAME_END_INFO;
    endInfo.displayTime = m_impl->displayTime;
    endInfo.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
    endInfo.layerCount = 1;
    endInfo.layers = layers;

    XrResult res = xrEndFrame(m_impl->session, &endInfo);
    m_impl->frameBegun = false;
    return (res == XR_SUCCESS);
}

} // namespace vr
} // namespace eyelib
