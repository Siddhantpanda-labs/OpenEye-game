#include "eyelib/render/rhi/OpenGL/DeviceGL.h"
#include "eyelib/render/rhi/OpenGL/BufferGL.h"
#include "eyelib/render/rhi/OpenGL/TextureGL.h"
#include "eyelib/render/rhi/OpenGL/ShaderGL.h"
#include "eyelib/render/rhi/OpenGL/PipelineGL.h"
#include "eyelib/render/rhi/OpenGL/RenderTargetGL.h"
#include "eyelib/render/rhi/OpenGL/QueryPoolGL.h"
#include "eyelib/render/rhi/OpenGL/GLEnums.h"
#include <glad/glad.h>
#include <cstring>

namespace eyelib {
namespace render {
namespace rhi {

DeviceGL::DeviceGL() {
    m_immediateCommandList = std::make_unique<CommandListGL>(this);
}

DeviceGL::~DeviceGL() {
    m_immediateCommandList.reset();
}

ICommandList* DeviceGL::GetImmediateCommandList() {
    return m_immediateCommandList.get();
}

void DeviceGL::SubmitCommandList(ICommandList* list) {
    (void)list;
    // OpenGL is immediate; Submit is no-op. For Vulkan we would submit to queue.
}

IBuffer* DeviceGL::CreateBuffer(const BufferDesc& desc, const void* initialData) {
    if (desc.sizeInBytes == 0) return nullptr;
    auto* buf = new BufferGL();
    buf->m_size = desc.sizeInBytes;
    buf->m_hint = desc.memoryHint;
    glCreateBuffers(1, &buf->m_handle);
    GLenum usage = (desc.memoryHint == BufferMemoryHint::HostVisible) ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW;
    if (initialData)
        glNamedBufferData(buf->m_handle, static_cast<GLsizeiptr>(desc.sizeInBytes), initialData, usage);
    else
        glNamedBufferData(buf->m_handle, static_cast<GLsizeiptr>(desc.sizeInBytes), nullptr, usage);
    return buf;
}

void DeviceGL::DestroyBuffer(IBuffer* buffer) {
    auto* b = static_cast<BufferGL*>(buffer);
    if (b && b->m_handle) {
        glDeleteBuffers(1, &b->m_handle);
        b->m_handle = 0;
    }
    delete b;
}

ITexture* DeviceGL::CreateTexture(const TextureDesc& desc, const void* initialData) {
    auto* tex = new TextureGL();
    tex->m_width = desc.width;
    tex->m_height = desc.height;
    tex->m_depthOrArray = desc.depthOrArrayLayers;
    tex->m_type = desc.type;
    tex->m_format = desc.format;

    GLenum target = GLEnums::TextureTypeToGL(desc.type);
    glCreateTextures(target, 1, &tex->m_handle);

    if (desc.type == TextureType::Texture2D) {
        glTextureStorage2D(tex->m_handle, desc.mipLevels,
                           GLEnums::TextureFormatToGLInternal(desc.format),
                           static_cast<GLsizei>(desc.width),
                           static_cast<GLsizei>(desc.height));
        if (initialData) {
            glTextureSubImage2D(tex->m_handle, 0, 0, 0,
                               static_cast<GLsizei>(desc.width),
                               static_cast<GLsizei>(desc.height),
                               GLEnums::TextureFormatToGLFormat(desc.format),
                               GLEnums::TextureFormatToGLDataType(desc.format),
                               initialData);
        }
    } else if (desc.type == TextureType::Texture2DArray) {
        glTextureStorage3D(tex->m_handle, desc.mipLevels,
                          GLEnums::TextureFormatToGLInternal(desc.format),
                          static_cast<GLsizei>(desc.width),
                          static_cast<GLsizei>(desc.height),
                          static_cast<GLsizei>(desc.depthOrArrayLayers));
    } else if (desc.type == TextureType::TextureCube) {
        glTextureStorage2D(tex->m_handle, desc.mipLevels,
                          GLEnums::TextureFormatToGLInternal(desc.format),
                          static_cast<GLsizei>(desc.width),
                          static_cast<GLsizei>(desc.height));
    } else if (desc.type == TextureType::Texture3D) {
        glTextureStorage3D(tex->m_handle, desc.mipLevels,
                          GLEnums::TextureFormatToGLInternal(desc.format),
                          static_cast<GLsizei>(desc.width),
                          static_cast<GLsizei>(desc.height),
                          static_cast<GLsizei>(desc.depthOrArrayLayers));
    }

    return tex;
}

void DeviceGL::DestroyTexture(ITexture* texture) {
    auto* t = static_cast<TextureGL*>(texture);
    if (t && t->m_handle && t->m_owned) {
        glDeleteTextures(1, &t->m_handle);
        t->m_handle = 0;
    }
    delete t;
}

ITexture* DeviceGL::CreateTextureWrapper(void* nativeTextureId, uint32_t width, uint32_t height, RHITextureFormat format) {
    if (!nativeTextureId) return nullptr;
    auto* tex = new TextureGL();
    tex->m_handle = static_cast<unsigned int>(reinterpret_cast<uintptr_t>(nativeTextureId));
    tex->m_width = width;
    tex->m_height = height;
    tex->m_type = TextureType::Texture2D;
    tex->m_format = format;
    tex->m_owned = false;
    return tex;
}

namespace {
unsigned int CompileShaderStage(ShaderStage stage, const char* source, size_t sourceLen) {
    GLenum glStage = GLEnums::ShaderStageToGL(stage);
    GLuint sh = glCreateShader(glStage);
    GLint len = static_cast<GLint>(sourceLen);
    glShaderSource(sh, 1, &source, &len);
    glCompileShader(sh);
    GLint ok = 0;
    glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetShaderInfoLog(sh, sizeof(log), nullptr, log);
        (void)log;  // TODO: report
        glDeleteShader(sh);
        return 0;
    }
    return sh;
}
} // namespace

IShader* DeviceGL::CreateShader(ShaderStage stage, const char* source, size_t sourceLen) {
    if (!source) return nullptr;
    size_t len = sourceLen;
    if (len == 0) len = std::strlen(source);
    unsigned int sh = CompileShaderStage(stage, source, len);
    if (!sh) return nullptr;
    auto* s = new ShaderGL();
    s->m_handle = sh;
    s->m_stage = stage;
    return s;
}

void DeviceGL::DestroyShader(IShader* shader) {
    auto* s = static_cast<ShaderGL*>(shader);
    if (s && s->m_handle) {
        glDeleteShader(s->m_handle);
        s->m_handle = 0;
    }
    delete s;
}

IPipeline* DeviceGL::CreateGraphicsPipeline(const PipelineDesc& desc) {
    ShaderGL* vs = static_cast<ShaderGL*>(desc.vertexShader);
    ShaderGL* fs = static_cast<ShaderGL*>(desc.fragmentShader);
    if (!vs || !fs || !vs->m_handle || !fs->m_handle) return nullptr;

    GLuint program = glCreateProgram();
    glAttachShader(program, vs->m_handle);
    glAttachShader(program, fs->m_handle);
    glLinkProgram(program);
    GLint ok = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetProgramInfoLog(program, sizeof(log), nullptr, log);
        (void)log;
        glDeleteProgram(program);
        return nullptr;
    }

    auto* p = new PipelineGL();
    p->m_program = program;
    p->m_vertexLayout = desc.vertexLayout;
    p->m_topology = desc.topology;
    p->m_blend = desc.blend;
    p->m_depthStencil = desc.depthStencil;
    p->m_raster = desc.raster;
    return p;
}

IPipeline* DeviceGL::CreateComputePipeline(void* computeShader) {
    (void)computeShader;
    return nullptr;  // TODO
}

void DeviceGL::DestroyPipeline(IPipeline* pipeline) {
    auto* p = static_cast<PipelineGL*>(pipeline);
    if (p && p->m_program) {
        glDeleteProgram(p->m_program);
        p->m_program = 0;
    }
    delete p;
}

IRenderTarget* DeviceGL::CreateRenderTarget(uint32_t width, uint32_t height,
                                            const std::vector<RHITextureFormat>& colorFormats,
                                            RHITextureFormat depthFormat,
                                            uint32_t samples) {
    auto* rt = new RenderTargetGL();
    rt->m_width = width;
    rt->m_height = height;

    glCreateFramebuffers(1, &rt->m_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, rt->m_fbo);

    for (size_t i = 0; i < colorFormats.size(); ++i) {
        TextureDesc td;
        td.type = TextureType::Texture2D;
        td.width = width;
        td.height = height;
        td.format = colorFormats[i];
        td.usage = TextureUsage::RenderTarget | TextureUsage::ShaderResource;
        td.samples = samples;
        auto* tex = static_cast<TextureGL*>(CreateTexture(td, nullptr));
        if (!tex) continue;
        rt->m_colorTextures.emplace_back(tex);
        glFramebufferTexture2D(GL_FRAMEBUFFER,
                              static_cast<GLenum>(GL_COLOR_ATTACHMENT0 + static_cast<int>(i)),
                              GL_TEXTURE_2D, tex->m_handle, 0);
    }

    if (depthFormat != RHITextureFormat::None) {
        TextureDesc td;
        td.type = TextureType::Texture2D;
        td.width = width;
        td.height = height;
        td.format = depthFormat;
        td.usage = TextureUsage::RenderTarget | TextureUsage::ShaderResource;
        td.samples = samples;
        rt->m_depthTexture.reset(static_cast<TextureGL*>(CreateTexture(td, nullptr)));
        GLenum att = (depthFormat == RHITextureFormat::Depth24Stencil8)
                     ? GL_DEPTH_STENCIL_ATTACHMENT : GL_DEPTH_ATTACHMENT;
        glFramebufferTexture2D(GL_FRAMEBUFFER, att, GL_TEXTURE_2D,
                               rt->m_depthTexture->m_handle, 0);
    }

    if (colorFormats.size() > 0) {
        std::vector<GLenum> drawBufs(colorFormats.size());
        for (size_t i = 0; i < colorFormats.size(); ++i)
            drawBufs[i] = GL_COLOR_ATTACHMENT0 + static_cast<GLenum>(i);
        glDrawBuffers(static_cast<GLsizei>(drawBufs.size()), drawBufs.data());
    }

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        DestroyRenderTarget(rt);
        return nullptr;
    }
    return rt;
}

void DeviceGL::DestroyRenderTarget(IRenderTarget* rt) {
    auto* r = static_cast<RenderTargetGL*>(rt);
    if (!r) return;
    if (r->m_fbo) {
        glDeleteFramebuffers(1, &r->m_fbo);
        r->m_fbo = 0;
    }
    for (auto& t : r->m_colorTextures) {
        if (t && t->m_handle) glDeleteTextures(1, &t->m_handle);
    }
    r->m_colorTextures.clear();
    if (r->m_depthTexture && r->m_depthTexture->m_handle)
        glDeleteTextures(1, &r->m_depthTexture->m_handle);
    r->m_depthTexture.reset();
    delete r;
}

IQueryPool* DeviceGL::CreateQueryPool(QueryType type, uint32_t count) {
    auto* pool = new QueryPoolGL();
    pool->m_type = type;
    pool->m_count = count;
    pool->m_queryIds.resize(count);
    if (count > 0)
        glGenQueries(static_cast<GLsizei>(count), pool->m_queryIds.data());
    return pool;
}

void DeviceGL::DestroyQueryPool(IQueryPool* pool) {
    auto* p = static_cast<QueryPoolGL*>(pool);
    if (p && !p->m_queryIds.empty()) {
        glDeleteQueries(static_cast<GLsizei>(p->m_queryIds.size()), p->m_queryIds.data());
        p->m_queryIds.clear();
    }
    delete p;
}

void DeviceGL::Present() {
    // Swap buffers is typically called by platform/window; no-op here or call glFinish.
}

void DeviceGL::BlitToDefaultFramebuffer(IRenderTarget* src) {
    if (!src) return;
    auto* r = static_cast<RenderTargetGL*>(src);
    BlitToDefaultFramebuffer(reinterpret_cast<void*>(static_cast<uintptr_t>(r->m_fbo)), src->GetWidth(), src->GetHeight());
}

void DeviceGL::BlitToDefaultFramebuffer(void* nativeFbo, uint32_t width, uint32_t height) {
    if (width == 0 || height == 0) return;
    GLuint fbo = (nativeFbo != nullptr) ? static_cast<GLuint>(reinterpret_cast<uintptr_t>(nativeFbo)) : 0;
    glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glBlitFramebuffer(0, 0, static_cast<GLint>(width), static_cast<GLint>(height),
                      0, 0, static_cast<GLint>(width), static_cast<GLint>(height),
                      GL_COLOR_BUFFER_BIT, GL_LINEAR);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
}

void DeviceGL::Blit(IRenderTarget* src, IRenderTarget* dst, bool colorOnly) {
    if (!src || !dst) return;
    auto* s = static_cast<RenderTargetGL*>(src);
    auto* d = static_cast<RenderTargetGL*>(dst);
    const uint32_t w = src->GetWidth();
    const uint32_t h = src->GetHeight();
    if (w == 0 || h == 0) return;
    glBindFramebuffer(GL_READ_FRAMEBUFFER, s->m_fbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, d->m_fbo);
    GLbitfield mask = colorOnly ? GL_COLOR_BUFFER_BIT : (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glBlitFramebuffer(0, 0, static_cast<GLint>(w), static_cast<GLint>(h),
                      0, 0, static_cast<GLint>(w), static_cast<GLint>(h), mask, GL_NEAREST);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
}

void DeviceGL::BlitFromNativeFramebuffer(void* nativeFbo, IRenderTarget* dst, bool colorOnly) {
    if (!dst) return;
    auto* d = static_cast<RenderTargetGL*>(dst);
    const uint32_t w = dst->GetWidth();
    const uint32_t h = dst->GetHeight();
    if (w == 0 || h == 0) return;
    GLuint srcFbo = (nativeFbo != nullptr) ? static_cast<GLuint>(reinterpret_cast<uintptr_t>(nativeFbo)) : 0;
    glBindFramebuffer(GL_READ_FRAMEBUFFER, srcFbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, d->m_fbo);
    GLbitfield mask = colorOnly ? GL_COLOR_BUFFER_BIT : (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glBlitFramebuffer(0, 0, static_cast<GLint>(w), static_cast<GLint>(h),
                      0, 0, static_cast<GLint>(w), static_cast<GLint>(h), mask, GL_NEAREST);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
}

} // namespace rhi
} // namespace render
} // namespace eyelib
