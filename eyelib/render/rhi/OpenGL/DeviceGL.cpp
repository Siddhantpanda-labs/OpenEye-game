#include "eyelib/render/rhi/OpenGL/DeviceGL.h"
#include "eyelib/render/rhi/OpenGL/DeferredCommandListGL.h"
#include "eyelib/render/rhi/OpenGL/BindlessHeapGL.h"
#include "eyelib/render/rhi/OpenGL/BufferGL.h"
#include "eyelib/render/rhi/OpenGL/TextureGL.h"
#include "eyelib/render/rhi/OpenGL/FenceGL.h"
#include "eyelib/render/rhi/OpenGL/ShaderGL.h"
#include "eyelib/render/rhi/OpenGL/PipelineGL.h"
#include "eyelib/render/rhi/OpenGL/PipelineLayoutGL.h"
#include "eyelib/render/rhi/OpenGL/DescriptorSetGL.h"
#include "eyelib/render/rhi/OpenGL/RenderTargetGL.h"
#include "eyelib/render/rhi/OpenGL/QueryPoolGL.h"
#include "eyelib/render/rhi/OpenGL/GLEnums.h"
#include "eyelib/render/ShaderSourceLoader.h"
#include "eyelib/core/Log.h"
#include "eyelib/platform/filesystem/FileSystem.h"
#include <glad/glad.h>
#include <algorithm>
#include <cstring>
#include <string>

namespace eyelib {
namespace render {
namespace rhi {

DeviceGL::DeviceGL() : m_caps{} {
    m_immediateCommandList = std::make_unique<CommandListGL>(this);
    QueryCapabilities();
}

DeviceGL::~DeviceGL() {
    if (m_fullscreenQuadVBO) { DestroyBuffer(m_fullscreenQuadVBO); m_fullscreenQuadVBO = nullptr; }
    if (m_fullscreenQuadIBO) { DestroyBuffer(m_fullscreenQuadIBO); m_fullscreenQuadIBO = nullptr; }
    m_immediateCommandList.reset();
}

void DeviceGL::EnsureFullscreenQuad() {
    if (m_fullscreenQuadVBO && m_fullscreenQuadIBO) return;
    const float vertices[] = {
        -1.f, -1.f, 0.f, 0.f,
         1.f, -1.f, 1.f, 0.f,
         1.f,  1.f, 1.f, 1.f,
        -1.f,  1.f, 0.f, 1.f,
    };
    const uint16_t indices[] = { 0, 1, 2, 2, 3, 0 };
    BufferDesc vboDesc;
    vboDesc.sizeInBytes = sizeof(vertices);
    vboDesc.usage = BufferUsage::Vertex;
    m_fullscreenQuadVBO = CreateBuffer(vboDesc, vertices);
    BufferDesc iboDesc;
    iboDesc.sizeInBytes = sizeof(indices);
    iboDesc.usage = BufferUsage::Index;
    m_fullscreenQuadIBO = CreateBuffer(iboDesc, indices);
}

void DeviceGL::QueryCapabilities() {
    m_caps = RHICapabilities{};
    // Core profile: glGetString(GL_EXTENSIONS) is invalid (GL_INVALID_ENUM). Use glGetStringi (3.0+).
    std::string extList;
    GLint numExt = 0;
    glGetIntegerv(GL_NUM_EXTENSIONS, &numExt);
    for (GLint i = 0; i < numExt; ++i) {
        const char* e = reinterpret_cast<const char*>(glGetStringi(GL_EXTENSIONS, static_cast<GLuint>(i)));
        if (e) {
            if (!extList.empty()) extList += ' ';
            extList += e;
        }
    }
    auto has = [&extList](const char* name) {
        std::string n(name);
        return extList.find(n) != std::string::npos;
    };
    m_caps.supportsBindless = has("GL_ARB_bindless_texture");
    m_caps.supportsCompute = has("GL_ARB_compute_shader");
    m_caps.supportsMultiDrawIndirect = has("GL_ARB_multi_draw_indirect");
    m_caps.supportsAsyncCompute = false;  // OpenGL has no async compute queue
    m_caps.supportsRayTracing = false;
    m_caps.supportsTessellation = has("GL_ARB_tessellation_shader");
    m_caps.supportsGeometryShader = has("GL_ARB_geometry_shader4") || has("GL_EXT_geometry_shader4");
    m_caps.supportsTransformFeedback = has("GL_ARB_transform_feedback2") || has("GL_EXT_transform_feedback");
    m_caps.supportsConditionalRender = has("GL_ARB_conditional_render") || has("GL_NV_conditional_render");
    m_caps.supportsSparseTexture = has("GL_ARB_sparse_texture");
    m_caps.supportsSparseBuffer = has("GL_ARB_sparse_buffer");
    m_caps.supportsAtomicCounters = has("GL_ARB_shader_atomic_counters");
    m_caps.supportsShaderSubroutines = has("GL_ARB_shader_subroutine");
}

const RHICapabilities& DeviceGL::GetCapabilities() const {
    return m_caps;
}

const char* DeviceGL::GetShaderBackendName() const {
    return "opengl";
}

const char* DeviceGL::GetShaderVertexExtension() const {
    return ".vert";
}

const char* DeviceGL::GetShaderFragmentExtension() const {
    return ".frag";
}

ICommandList* DeviceGL::CreateCommandList() {
    return new DeferredCommandListGL(this);
}

void DeviceGL::DestroyCommandList(ICommandList* list) {
    if (list && list != m_immediateCommandList.get())
        delete static_cast<DeferredCommandListGL*>(list);
}

ICommandList* DeviceGL::GetImmediateCommandList() {
    return m_immediateCommandList.get();
}

void DeviceGL::SubmitCommandList(ICommandList* list) {
    if (!list || list == m_immediateCommandList.get())
        return;
    auto* deferred = static_cast<DeferredCommandListGL*>(list);
    deferred->Replay(m_immediateCommandList.get());
}

IBuffer* DeviceGL::CreateBuffer(const BufferDesc& desc, const void* initialData) {
    if (desc.sizeInBytes == 0) return nullptr;
    auto* buf = new BufferGL();
    buf->m_size = desc.sizeInBytes;
    buf->m_hint = desc.memoryHint;
    glCreateBuffers(1, &buf->m_handle);
    const bool usePersistent = (desc.memoryHint == BufferMemoryHint::HostVisiblePersistent) && GLAD_GL_ARB_buffer_storage;
    const bool useSparse = desc.sparse && m_caps.supportsSparseBuffer;
    buf->m_persistent = usePersistent;
    buf->m_sparse = useSparse;
    if (useSparse) {
        glNamedBufferStorage(buf->m_handle, static_cast<GLsizeiptr>(desc.sizeInBytes), nullptr, GL_SPARSE_STORAGE_BIT_ARB);
    } else if (usePersistent) {
        glNamedBufferStorage(buf->m_handle, static_cast<GLsizeiptr>(desc.sizeInBytes), initialData,
                             GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT | GL_DYNAMIC_STORAGE_BIT);
    } else {
        GLenum usage = (desc.memoryHint == BufferMemoryHint::HostVisible) ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW;
        if (initialData)
            glNamedBufferData(buf->m_handle, static_cast<GLsizeiptr>(desc.sizeInBytes), initialData, usage);
        else
            glNamedBufferData(buf->m_handle, static_cast<GLsizeiptr>(desc.sizeInBytes), nullptr, usage);
    }
    return buf;
}

void DeviceGL::DestroyBuffer(IBuffer* buffer) {
    auto* b = static_cast<BufferGL*>(buffer);
    if (b && b->m_handle) {
        b->UnmapPersistent();
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
    if (desc.sparse && m_caps.supportsSparseTexture)
        glTextureParameteri(tex->m_handle, GL_TEXTURE_SPARSE_ARB, GL_TRUE);

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
        if (initialData) {
            glTextureSubImage3D(tex->m_handle, 0, 0, 0, 0,
                               static_cast<GLsizei>(desc.width),
                               static_cast<GLsizei>(desc.height),
                               static_cast<GLsizei>(desc.depthOrArrayLayers),
                               GLEnums::TextureFormatToGLFormat(desc.format),
                               GLEnums::TextureFormatToGLDataType(desc.format),
                               initialData);
        }
    } else if (desc.type == TextureType::TextureCube) {
        glTextureStorage2D(tex->m_handle, desc.mipLevels,
                          GLEnums::TextureFormatToGLInternal(desc.format),
                          static_cast<GLsizei>(desc.width),
                          static_cast<GLsizei>(desc.height));
        if (initialData) {
            glTextureSubImage3D(tex->m_handle, 0, 0, 0, 0,
                               static_cast<GLsizei>(desc.width),
                               static_cast<GLsizei>(desc.height),
                               6,
                               GLEnums::TextureFormatToGLFormat(desc.format),
                               GLEnums::TextureFormatToGLDataType(desc.format),
                               initialData);
        }
    } else if (desc.type == TextureType::Texture3D) {
        glTextureStorage3D(tex->m_handle, desc.mipLevels,
                          GLEnums::TextureFormatToGLInternal(desc.format),
                          static_cast<GLsizei>(desc.width),
                          static_cast<GLsizei>(desc.height),
                          static_cast<GLsizei>(desc.depthOrArrayLayers));
        if (initialData) {
            glTextureSubImage3D(tex->m_handle, 0, 0, 0, 0,
                               static_cast<GLsizei>(desc.width),
                               static_cast<GLsizei>(desc.height),
                               static_cast<GLsizei>(desc.depthOrArrayLayers),
                               GLEnums::TextureFormatToGLFormat(desc.format),
                               GLEnums::TextureFormatToGLDataType(desc.format),
                               initialData);
        }
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

void DeviceGL::CommitSparseTexturePages(ITexture* texture, uint32_t level, int32_t x, int32_t y, int32_t z, uint32_t width, uint32_t height, uint32_t depth, bool commit) {
    if (!texture || !m_caps.supportsSparseTexture) return;
    auto* t = static_cast<TextureGL*>(texture);
    if (!t->m_handle) return;
    GLenum target = GLEnums::TextureTypeToGL(t->m_type);
    glBindTexture(target, t->m_handle);
    glTexPageCommitmentARB(target, static_cast<GLint>(level), x, y, z,
        static_cast<GLsizei>(width), static_cast<GLsizei>(height), static_cast<GLsizei>(depth),
        commit ? GL_TRUE : GL_FALSE);
    glBindTexture(target, 0);
}

void DeviceGL::CommitSparseBufferPages(IBuffer* buffer, size_t offset, size_t size, bool commit) {
    if (!buffer || !m_caps.supportsSparseBuffer) return;
    auto* b = static_cast<BufferGL*>(buffer);
    if (!b->m_handle || !b->m_sparse) return;
    glNamedBufferPageCommitmentARB(b->m_handle, static_cast<GLintptr>(offset), static_cast<GLsizeiptr>(size), commit ? GL_TRUE : GL_FALSE);
}

ITexture* DeviceGL::CreateTextureView(ITexture* source, RHITextureFormat viewFormat, uint32_t baseMipLevel, uint32_t mipLevels, uint32_t baseLayer, uint32_t layerCount) {
    if (!source) return nullptr;
    auto* src = static_cast<TextureGL*>(source);
    if (!src->m_handle) return nullptr;
    GLenum target = GLEnums::TextureTypeToGL(src->m_type);
    GLuint viewHandle = 0;
    glGenTextures(1, &viewHandle);
    glTextureView(viewHandle, target, src->m_handle,
        GLEnums::TextureFormatToGLInternal(viewFormat),
        baseMipLevel, static_cast<GLsizei>(mipLevels), baseLayer, static_cast<GLsizei>(layerCount));
    auto* tex = new TextureGL();
    tex->m_handle = viewHandle;
    tex->m_width = src->m_width;
    tex->m_height = src->m_height;
    tex->m_depthOrArray = src->m_depthOrArray;
    tex->m_type = src->m_type;
    tex->m_format = viewFormat;
    tex->m_owned = true;
    tex->m_isView = true;
    return tex;
}

ITexture* DeviceGL::CreateTextureBuffer(IBuffer* buffer, RHITextureFormat format, size_t offset, size_t size) {
    if (!buffer || size == 0) return nullptr;
    auto* buf = static_cast<BufferGL*>(buffer);
    if (!buf->m_handle) return nullptr;
    GLuint texHandle = 0;
    glCreateTextures(GL_TEXTURE_BUFFER, 1, &texHandle);
    glTextureBufferRange(texHandle, GLEnums::TextureFormatToGLInternal(format),
        buf->m_handle, static_cast<GLintptr>(offset), static_cast<GLsizeiptr>(size));
    auto* tex = new TextureGL();
    tex->m_handle = texHandle;
    tex->m_width = static_cast<uint32_t>(size / 4);  // approximate; TBO is 1D
    tex->m_height = 1;
    tex->m_depthOrArray = 1;
    tex->m_type = TextureType::TextureBuffer;
    tex->m_format = format;
    tex->m_owned = true;
    tex->m_isTextureBuffer = true;
    return tex;
}

bool DeviceGL::WaitFence(IFence* fence, uint64_t timeoutMs) {
    if (!fence) return true;
    auto* f = static_cast<FenceGL*>(fence);
    if (!f->m_sync) return true;
    GLuint64 timeoutNs = (timeoutMs == 0) ? 0 : (timeoutMs == UINT64_MAX ? GL_TIMEOUT_IGNORED : static_cast<GLuint64>(timeoutMs) * 1000000u);
    GLenum waitRet = glClientWaitSync(static_cast<GLsync>(f->m_sync), GL_SYNC_FLUSH_COMMANDS_BIT, timeoutNs);
    return (waitRet == GL_ALREADY_SIGNALED || waitRet == GL_CONDITION_SATISFIED);
}

void DeviceGL::DestroyFence(IFence* fence) {
    if (!fence) return;
    auto* f = static_cast<FenceGL*>(fence);
    if (f->m_sync) {
        glDeleteSync(static_cast<GLsync>(f->m_sync));
        f->m_sync = nullptr;
    }
    delete f;
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
        const char* stageName = (stage == ShaderStage::Vertex) ? "vertex" : (stage == ShaderStage::Fragment) ? "fragment" : "compute";
        core::Log::ErrorF("RHI OpenGL: shader compile failed (%s): %s", stageName, log);
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

IShader* DeviceGL::CreateShaderFromBinary(ShaderStage stage, const void* data, size_t size) {
    (void)stage;
    (void)data;
    (void)size;
    return nullptr;
}

bool DeviceGL::TryLoadShaderFromPaths(const char* vertexPath, const char* fragmentPath, IShader*& outVs, IShader*& outFs) {
    (void)vertexPath;
    (void)fragmentPath;
    outVs = nullptr;
    outFs = nullptr;
    return false;
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

    ShaderGL* tcs = static_cast<ShaderGL*>(desc.tessControlShader);
    ShaderGL* tes = static_cast<ShaderGL*>(desc.tessEvalShader);
    ShaderGL* gs = static_cast<ShaderGL*>(desc.geometryShader);
    if ((tcs && !tes) || (!tcs && tes)) return nullptr;  // tessellation requires both control and eval

    GLuint program = glCreateProgram();
    glAttachShader(program, vs->m_handle);
    glAttachShader(program, fs->m_handle);
    if (tcs && tcs->m_handle) glAttachShader(program, tcs->m_handle);
    if (tes && tes->m_handle) glAttachShader(program, tes->m_handle);
    if (gs && gs->m_handle) glAttachShader(program, gs->m_handle);

    if (!desc.transformFeedbackVaryings.empty() && m_caps.supportsTransformFeedback) {
        glTransformFeedbackVaryings(program, static_cast<GLsizei>(desc.transformFeedbackVaryings.size()),
            desc.transformFeedbackVaryings.data(), GL_INTERLEAVED_ATTRIBS);
    }
    glLinkProgram(program);
    GLint ok = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetProgramInfoLog(program, sizeof(log), nullptr, log);
        core::Log::ErrorF("RHI OpenGL: graphics pipeline link failed: %s", log);
        glDeleteProgram(program);
        return nullptr;
    }

    auto* p = new PipelineGL();
    p->m_program = program;
    p->m_vertexLayout = desc.vertexLayout;
    p->m_topology = (tcs && tes) ? PrimitiveTopology::PatchList : desc.topology;
    p->m_patchVertexCount = desc.patchVertexCount;
    p->m_hasTessellation = (tcs && tes && tcs->m_handle && tes->m_handle);
    p->m_hasGeometry = (gs && gs->m_handle);
    p->m_blend = desc.blend;
    p->m_depthStencil = desc.depthStencil;
    p->m_raster = desc.raster;
    return p;
}

IPipeline* DeviceGL::CreateGraphicsPipelineFromName(const char* shaderName, const VertexLayout& layout, PrimitiveTopology topology) {
    if (!shaderName || !shaderName[0]) return nullptr;
    // Resolve logical name to OpenGL GLSL: assets/shaders/opengl/{name}.vert/.frag (exe dir preferred)
    std::string baseDir = "assets/shaders/opengl/";
    std::string vertPath = baseDir + shaderName + ".vert";
    std::string fragPath = baseDir + shaderName + ".frag";
    std::string exeDir = eyelib::platform::filesystem::GetDirectory(eyelib::platform::filesystem::GetExecutablePath());
    if (!exeDir.empty() && eyelib::platform::filesystem::FileExists(exeDir + vertPath)) {
        vertPath = exeDir + vertPath;
        fragPath = exeDir + fragPath;
    }

    std::string vertexSrc;
    if (!eyelib::render::LoadShaderSource(vertPath, vertexSrc)) return nullptr;
    std::string fragmentSrc;
    if (!eyelib::render::LoadShaderSource(fragPath, fragmentSrc)) return nullptr;

    IShader* vs = CreateShader(ShaderStage::Vertex, vertexSrc.c_str(), vertexSrc.size());
    IShader* fs = CreateShader(ShaderStage::Fragment, fragmentSrc.c_str(), fragmentSrc.size());
    if (!vs || !fs) {
        if (vs) DestroyShader(vs);
        if (fs) DestroyShader(fs);
        return nullptr;
    }

    PipelineDesc desc;
    desc.vertexShader = vs;
    desc.fragmentShader = fs;
    desc.vertexLayout = layout;
    desc.topology = topology;
    desc.depthStencil.depthTest = true;
    desc.depthStencil.depthWrite = true;
    IPipeline* pipe = CreateGraphicsPipeline(desc);
    DestroyShader(vs);
    DestroyShader(fs);
    return pipe;
}

bool DeviceGL::GetPipelineBinary(IPipeline* pipeline, std::vector<uint8_t>& outData, uint32_t& outFormat) {
    auto* p = static_cast<PipelineGL*>(pipeline);
    if (!p || !p->m_program) return false;
    GLint len = 0;
    glGetProgramiv(p->m_program, GL_PROGRAM_BINARY_LENGTH, &len);
    if (len <= 0) return false;
    outData.resize(static_cast<size_t>(len));
    GLsizei written = 0;
    GLenum fmt = 0;
    glGetProgramBinary(p->m_program, len, &written, &fmt, outData.data());
    if (written <= 0) return false;
    outData.resize(static_cast<size_t>(written));
    outFormat = static_cast<uint32_t>(fmt);
    return true;
}

IPipeline* DeviceGL::CreateGraphicsPipelineFromBinary(const void* data, size_t size, uint32_t binaryFormat, const VertexLayout& layout, PrimitiveTopology topology) {
    if (!data || size == 0) return nullptr;
    GLuint program = glCreateProgram();
    glProgramBinary(program, static_cast<GLenum>(binaryFormat), data, static_cast<GLsizei>(size));
    GLint ok = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (!ok) {
        glDeleteProgram(program);
        return nullptr;
    }
    auto* p = new PipelineGL();
    p->m_program = program;
    p->m_vertexLayout = layout;
    p->m_topology = topology;
    p->m_depthStencil.depthTest = true;
    p->m_depthStencil.depthWrite = true;
    return p;
}

IPipeline* DeviceGL::CreateComputePipeline(void* computeShader) {
    ShaderGL* cs = static_cast<ShaderGL*>(computeShader);
    if (!cs || !cs->m_handle || cs->GetStage() != ShaderStage::Compute) return nullptr;

    GLuint program = glCreateProgram();
    glAttachShader(program, cs->m_handle);
    glLinkProgram(program);
    GLint ok = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetProgramInfoLog(program, sizeof(log), nullptr, log);
        core::Log::ErrorF("RHI OpenGL: compute pipeline link failed: %s", log);
        glDeleteProgram(program);
        return nullptr;
    }

    // Explicitly set SSBO block bindings so driver interface matches (avoids GL 1282 at bind time).
    GLuint idx0 = glGetProgramResourceIndex(program, GL_SHADER_STORAGE_BLOCK, "IndirectOut");
    if (idx0 == GL_INVALID_INDEX) {
        core::Log::Warn("RHI OpenGL: compute program has no SSBO block 'IndirectOut' (optimized out or missing); GL 1282 may occur at bind.");
    } else {
        glShaderStorageBlockBinding(program, idx0, 0);
    }
    GLuint idx1 = glGetProgramResourceIndex(program, GL_SHADER_STORAGE_BLOCK, "DrawListIn");
    if (idx1 == GL_INVALID_INDEX) {
        core::Log::Warn("RHI OpenGL: compute program has no SSBO block 'DrawListIn' (optimized out or missing); do not bind slot 1 to avoid GL 1282.");
    } else {
        glShaderStorageBlockBinding(program, idx1, 1);
    }

    auto* p = new PipelineGL();
    p->m_program = program;
    return p;
}

void DeviceGL::DestroyPipeline(IPipeline* pipeline) {
    auto* p = static_cast<PipelineGL*>(pipeline);
    if (p && p->m_program) {
        glDeleteProgram(p->m_program);
        p->m_program = 0;
    }
    delete p;
}

void DeviceGL::EnsureComputePipelineStorageBlockBindings(IPipeline* pipeline) {
    if (!pipeline) return;
    auto* p = static_cast<PipelineGL*>(pipeline);
    if (!p->m_program) return;
    GLuint program = p->m_program;
    GLuint idx0 = glGetProgramResourceIndex(program, GL_SHADER_STORAGE_BLOCK, "IndirectOut");
    if (idx0 != GL_INVALID_INDEX)
        glShaderStorageBlockBinding(program, idx0, 0);
    GLuint idx1 = glGetProgramResourceIndex(program, GL_SHADER_STORAGE_BLOCK, "DrawListIn");
    if (idx1 != GL_INVALID_INDEX)
        glShaderStorageBlockBinding(program, idx1, 1);
}

bool DeviceGL::ComputePipelineHasStorageBlock(IPipeline* pipeline, const char* blockName) {
    if (!pipeline || !blockName) return false;
    auto* p = static_cast<PipelineGL*>(pipeline);
    if (!p->m_program) return false;
    GLuint idx = glGetProgramResourceIndex(p->m_program, GL_SHADER_STORAGE_BLOCK, blockName);
    return (idx != GL_INVALID_INDEX);
}

IRenderTarget* DeviceGL::CreateRenderTarget(uint32_t width, uint32_t height,
                                            const std::vector<RHITextureFormat>& colorFormats,
                                            RHITextureFormat depthFormat,
                                            uint32_t samples) {
    auto* rt = new RenderTargetGL();
    rt->m_width = width;
    rt->m_height = height;
    rt->m_ownsTextures = true;

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
    } else if (depthFormat != RHITextureFormat::None) {
        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);
    }

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        DestroyRenderTarget(rt);
        return nullptr;
    }
    return rt;
}

IRenderTarget* DeviceGL::CreateRenderTargetFromTexture(ITexture* colorTexture, uint32_t face, uint32_t mipLevel) {
    auto* tex = static_cast<TextureGL*>(colorTexture);
    if (!tex || tex->m_handle == 0) return nullptr;

    auto* rt = new RenderTargetGL();
    rt->m_ownsTextures = false;
    rt->m_externalColorTextures.push_back(tex);

    uint32_t mipWidth = std::max<uint32_t>(1u, tex->m_width >> mipLevel);
    uint32_t mipHeight = std::max<uint32_t>(1u, tex->m_height >> mipLevel);
    rt->m_width = mipWidth;
    rt->m_height = mipHeight;

    glCreateFramebuffers(1, &rt->m_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, rt->m_fbo);

    GLenum target = GL_TEXTURE_2D;
    if (tex->m_type == TextureType::TextureCube) {
        uint32_t faceIndex = std::min<uint32_t>(face, 5u);
        target = static_cast<GLenum>(GL_TEXTURE_CUBE_MAP_POSITIVE_X + faceIndex);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, target, tex->m_handle, static_cast<GLint>(mipLevel));
    } else if (tex->m_type == TextureType::Texture2D) {
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, target, tex->m_handle, static_cast<GLint>(mipLevel));
    } else {
        glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, tex->m_handle, static_cast<GLint>(mipLevel),
                                  static_cast<GLint>(face));
    }

    GLenum drawBufs[1] = { GL_COLOR_ATTACHMENT0 };
    glDrawBuffers(1, drawBufs);

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
    if (r->m_ownsTextures) {
        for (auto& t : r->m_colorTextures) {
            if (t && t->m_handle) glDeleteTextures(1, &t->m_handle);
        }
    }
    r->m_colorTextures.clear();
    if (r->m_ownsTextures && r->m_depthTexture && r->m_depthTexture->m_handle)
        glDeleteTextures(1, &r->m_depthTexture->m_handle);
    r->m_depthTexture.reset();
    r->m_externalColorTextures.clear();
    r->m_externalDepthTexture = nullptr;
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

IPipelineLayout* DeviceGL::CreatePipelineLayout(const PipelineLayoutDesc& desc) {
    auto* layout = new PipelineLayoutGL();
    layout->desc = desc;
    return layout;
}

void DeviceGL::DestroyPipelineLayout(IPipelineLayout* layout) {
    delete static_cast<PipelineLayoutGL*>(layout);
}

IDescriptorSet* DeviceGL::CreateDescriptorSet(IPipelineLayout* layout) {
    if (!layout) return nullptr;
    auto* pl = static_cast<PipelineLayoutGL*>(layout);
    auto* set = new DescriptorSetGL();
    set->layout = pl;
    return set;
}

void DeviceGL::DestroyDescriptorSet(IDescriptorSet* set) {
    delete static_cast<DescriptorSetGL*>(set);
}

IBindlessHeap* DeviceGL::CreateBindlessHeap() {
    if (!GetCapabilities().supportsBindless) return nullptr;
    return new BindlessHeapGL();
}

void DeviceGL::DestroyBindlessHeap(IBindlessHeap* heap) {
    delete static_cast<BindlessHeapGL*>(heap);
}

int DeviceGL::GetMaxSamples(int requestedSamples) {
    if (requestedSamples <= 0) return 0;
    GLint maxSamples = 0;
    glGetIntegerv(GL_MAX_SAMPLES, &maxSamples);
    return static_cast<int>(std::min(static_cast<GLint>(requestedSamples), maxSamples));
}

void DeviceGL::Present() {
    // Swap buffers is typically called by platform/window; no-op here.
}

void DeviceGL::Finish() {
    glFinish();
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

void DeviceGL::BlitToDefaultFramebufferLetterbox(IRenderTarget* src, uint32_t windowWidth, uint32_t windowHeight) {
    if (!src || windowWidth == 0 || windowHeight == 0) return;
    const uint32_t srcW = src->GetWidth();
    const uint32_t srcH = src->GetHeight();
    if (srcW == 0 || srcH == 0) return;
    // Cover mode: fill full window, preserve scene aspect by cropping (no clear-color bars).
    float scale = static_cast<float>(windowWidth) / static_cast<float>(srcW);
    float scaleH = static_cast<float>(windowHeight) / static_cast<float>(srcH);
    if (scaleH > scale) scale = scaleH;
    float srcVisibleW = static_cast<float>(windowWidth) / scale;
    float srcVisibleH = static_cast<float>(windowHeight) / scale;
    srcVisibleW = std::min(srcVisibleW, static_cast<float>(srcW));
    srcVisibleH = std::min(srcVisibleH, static_cast<float>(srcH));
    GLint srcX = static_cast<GLint>((static_cast<float>(srcW) - srcVisibleW) * 0.5f);
    GLint srcY = static_cast<GLint>((static_cast<float>(srcH) - srcVisibleH) * 0.5f);
    if (srcX < 0) srcX = 0;
    if (srcY < 0) srcY = 0;
    GLint srcR = std::min(srcX + static_cast<GLint>(srcVisibleW), static_cast<GLint>(srcW));
    GLint srcB = std::min(srcY + static_cast<GLint>(srcVisibleH), static_cast<GLint>(srcH));
    auto* r = static_cast<RenderTargetGL*>(src);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, r->m_fbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glBlitFramebuffer(srcX, srcY, srcR, srcB,
                      0, 0, static_cast<GLint>(windowWidth), static_cast<GLint>(windowHeight),
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

void DeviceGL::BlitDepthOnly(IRenderTarget* src, IRenderTarget* dst) {
    if (!src || !dst) return;
    void* nativeFbo = src->GetNativeFramebuffer();
    if (!nativeFbo) return;
    auto* d = static_cast<RenderTargetGL*>(dst);
    const uint32_t w = dst->GetWidth();
    const uint32_t h = dst->GetHeight();
    if (w == 0 || h == 0) return;
    GLuint srcFbo = static_cast<GLuint>(reinterpret_cast<uintptr_t>(nativeFbo));
    glBindFramebuffer(GL_READ_FRAMEBUFFER, srcFbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, d->m_fbo);
    glBlitFramebuffer(0, 0, static_cast<GLint>(w), static_cast<GLint>(h),
                      0, 0, static_cast<GLint>(w), static_cast<GLint>(h), GL_DEPTH_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
}

} // namespace rhi
} // namespace render
} // namespace eyelib
