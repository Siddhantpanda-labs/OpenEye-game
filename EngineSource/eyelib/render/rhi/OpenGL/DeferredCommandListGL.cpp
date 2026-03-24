#include "eyelib/render/rhi/OpenGL/DeferredCommandListGL.h"
#include "eyelib/render/rhi/OpenGL/DeviceGL.h"
#include <cstring>
#include <string>
#include <vector>

namespace eyelib {
namespace render {
namespace rhi {

DeferredCommandListGL::DeferredCommandListGL(DeviceGL* device) : m_device(device) {}

DeferredCommandListGL::~DeferredCommandListGL() = default;

void DeferredCommandListGL::Record(CommandFn fn) {
    if (!m_recording) return;
    m_commands.push_back(std::move(fn));
}

void DeferredCommandListGL::Begin() {
    m_commands.clear();
    m_recording = true;
}

void DeferredCommandListGL::End() {
    m_recording = false;
}

void DeferredCommandListGL::Replay(ICommandList* target) {
    for (auto& fn : m_commands)
        fn(target);
}

void DeferredCommandListGL::SetRenderTarget(IRenderTarget* rt) {
    Record([rt](ICommandList* c) { c->SetRenderTarget(rt); });
}
void DeferredCommandListGL::SetRenderTarget(IRenderTarget* rt, uint32_t colorAttachmentIndex) {
    Record([rt, colorAttachmentIndex](ICommandList* c) { c->SetRenderTarget(rt, colorAttachmentIndex); });
}
void DeferredCommandListGL::ClearRenderTarget(IRenderTarget* rt, const ClearValue& clear) {
    ClearValue copy = clear;
    Record([rt, copy](ICommandList* c) { c->ClearRenderTarget(rt, copy); });
}
void DeferredCommandListGL::ClearDefaultFramebuffer(const ClearValue& clear) {
    ClearValue copy = clear;
    Record([copy](ICommandList* c) { c->ClearDefaultFramebuffer(copy); });
}
void DeferredCommandListGL::SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height) {
    Record([x, y, width, height](ICommandList* c) { c->SetViewport(x, y, width, height); });
}
void DeferredCommandListGL::SetViewportIndexed(uint32_t index, uint32_t x, uint32_t y, uint32_t width, uint32_t height) {
    Record([index, x, y, width, height](ICommandList* c) { c->SetViewportIndexed(index, x, y, width, height); });
}
void DeferredCommandListGL::SetDepthTest(bool enable) {
    Record([enable](ICommandList* c) { c->SetDepthTest(enable); });
}
void DeferredCommandListGL::SetDepthWrite(bool enable) {
    Record([enable](ICommandList* c) { c->SetDepthWrite(enable); });
}
void DeferredCommandListGL::SetDepthFuncLess(bool less) {
    Record([less](ICommandList* c) { c->SetDepthFuncLess(less); });
}
void DeferredCommandListGL::SetCullFace(bool enable) {
    Record([enable](ICommandList* c) { c->SetCullFace(enable); });
}
void DeferredCommandListGL::SetCullFrontFace(bool cullFront) {
    Record([cullFront](ICommandList* c) { c->SetCullFrontFace(cullFront); });
}
void DeferredCommandListGL::SetBlend(bool enable) {
    Record([enable](ICommandList* c) { c->SetBlend(enable); });
}
void DeferredCommandListGL::SetBlendFunc(BlendFactor src, BlendFactor dst) {
    Record([src, dst](ICommandList* c) { c->SetBlendFunc(src, dst); });
}
void DeferredCommandListGL::SetPolygonOffsetLine(bool enable, float factor, float units) {
    Record([enable, factor, units](ICommandList* c) { c->SetPolygonOffsetLine(enable, factor, units); });
}

void DeferredCommandListGL::BarrierBuffer(IBuffer* buffer, ResourceState fromState, ResourceState toState) {
    Record([buffer, fromState, toState](ICommandList* c) { c->BarrierBuffer(buffer, fromState, toState); });
}
void DeferredCommandListGL::BarrierTexture(ITexture* texture, ResourceState fromState, ResourceState toState) {
    Record([texture, fromState, toState](ICommandList* c) { c->BarrierTexture(texture, fromState, toState); });
}
void DeferredCommandListGL::MemoryBarrierStorage() {
    Record([](ICommandList* c) { c->MemoryBarrierStorage(); });
}
void DeferredCommandListGL::BindPixelPackBufferAsSSBO(void* pboHandle, uint32_t slot, size_t size) {
    Record([pboHandle, slot, size](ICommandList* c) { c->BindPixelPackBufferAsSSBO(pboHandle, slot, size); });
}

void DeferredCommandListGL::BindPipeline(IPipeline* pipeline) {
    Record([pipeline](ICommandList* c) { c->BindPipeline(pipeline); });
}
void DeferredCommandListGL::BindVertexBuffer(IBuffer* buffer, uint32_t binding, uint32_t offset) {
    Record([buffer, binding, offset](ICommandList* c) { c->BindVertexBuffer(buffer, binding, offset); });
}
void DeferredCommandListGL::DrawFullscreenQuad(IPipeline* pipeline) {
    Record([pipeline](ICommandList* c) { c->DrawFullscreenQuad(pipeline); });
}
void DeferredCommandListGL::BindIndexBuffer(IBuffer* buffer, IndexType indexType, uint32_t offset) {
    Record([buffer, indexType, offset](ICommandList* c) { c->BindIndexBuffer(buffer, indexType, offset); });
}
void DeferredCommandListGL::BindUniformBuffer(IBuffer* buffer, uint32_t binding, uint32_t offset, uint32_t size) {
    Record([buffer, binding, offset, size](ICommandList* c) { c->BindUniformBuffer(buffer, binding, offset, size); });
}
void DeferredCommandListGL::BindStorageBuffer(IBuffer* buffer, uint32_t binding, uint32_t offset, uint32_t size) {
    Record([buffer, binding, offset, size](ICommandList* c) { c->BindStorageBuffer(buffer, binding, offset, size); });
}
void DeferredCommandListGL::BindAtomicCounterBuffer(uint32_t slot, IBuffer* buffer, size_t offset, size_t size) {
    Record([slot, buffer, offset, size](ICommandList* c) { c->BindAtomicCounterBuffer(slot, buffer, offset, size); });
}
void DeferredCommandListGL::BindTexture(ITexture* texture, uint32_t binding) {
    Record([texture, binding](ICommandList* c) { c->BindTexture(texture, binding); });
}
void DeferredCommandListGL::UnbindTexture(uint32_t binding) {
    Record([binding](ICommandList* c) { c->UnbindTexture(binding); });
}
void DeferredCommandListGL::BindDescriptorSet(IDescriptorSet* set) {
    Record([set](ICommandList* c) { c->BindDescriptorSet(set); });
}

void DeferredCommandListGL::BindDescriptorSets(uint32_t firstSet, uint32_t count, IDescriptorSet* const* sets,
                                               const uint32_t* dynamicOffsets, uint32_t dynamicOffsetCount) {
    std::vector<IDescriptorSet*> setCopy;
    std::vector<uint32_t> dynCopy;
    if (sets && count > 0)
        setCopy.assign(sets, sets + count);
    if (dynamicOffsets && dynamicOffsetCount > 0)
        dynCopy.assign(dynamicOffsets, dynamicOffsets + dynamicOffsetCount);
    Record([firstSet, setCopy, dynCopy](ICommandList* c) {
        c->BindDescriptorSets(firstSet, static_cast<uint32_t>(setCopy.size()),
            setCopy.empty() ? nullptr : setCopy.data(),
            dynCopy.empty() ? nullptr : dynCopy.data(), static_cast<uint32_t>(dynCopy.size()));
    });
}
void DeferredCommandListGL::BindBindlessHeap(IBindlessHeap* heap) {
    Record([heap](ICommandList* c) { c->BindBindlessHeap(heap); });
}

void DeferredCommandListGL::SetPushConstants(uint32_t offset, uint32_t size, const void* data) {
    std::vector<uint8_t> copy(size);
    if (data && size) std::memcpy(copy.data(), data, size);
    Record([offset, size, copy](ICommandList* c) { c->SetPushConstants(offset, size, copy.data()); });
}

void DeferredCommandListGL::SetUniformInt(const char* name, int value) {
    std::string n(name ? name : "");
    Record([n, value](ICommandList* c) { c->SetUniformInt(n.c_str(), value); });
}
void DeferredCommandListGL::SetUniformUint(const char* name, uint32_t value) {
    std::string n(name ? name : "");
    Record([n, value](ICommandList* c) { c->SetUniformUint(n.c_str(), value); });
}
void DeferredCommandListGL::SetUniformFloat(const char* name, float value) {
    std::string n(name ? name : "");
    Record([n, value](ICommandList* c) { c->SetUniformFloat(n.c_str(), value); });
}
void DeferredCommandListGL::SetUniformVec2(const char* name, const float* data) {
    std::string n(name ? name : "");
    float d[2] = { data ? data[0] : 0.f, data ? data[1] : 0.f };
    Record([n, d](ICommandList* c) { c->SetUniformVec2(n.c_str(), d); });
}
void DeferredCommandListGL::SetUniformVec3(const char* name, const float* data) {
    std::string n(name ? name : "");
    float d[3] = { data ? data[0] : 0.f, data ? data[1] : 0.f, data ? data[2] : 0.f };
    Record([n, d](ICommandList* c) { c->SetUniformVec3(n.c_str(), d); });
}
void DeferredCommandListGL::SetUniformVec4(const char* name, const float* data) {
    std::string n(name ? name : "");
    float d[4] = { data ? data[0] : 0.f, data ? data[1] : 0.f, data ? data[2] : 0.f, data ? data[3] : 0.f };
    Record([n, d](ICommandList* c) { c->SetUniformVec4(n.c_str(), d); });
}
void DeferredCommandListGL::SetUniformMat4(const char* name, const float* data) {
    std::string n(name ? name : "");
    float d[16];
    if (data) std::memcpy(d, data, sizeof(d)); else std::memset(d, 0, sizeof(d));
    Record([n, d](ICommandList* c) { c->SetUniformMat4(n.c_str(), d); });
}
void DeferredCommandListGL::SetUniformMat4Array(const char* name, const float* data, size_t count) {
    std::string n(name ? name : "");
    std::vector<float> copy(count * 16);
    if (data && count) std::memcpy(copy.data(), data, copy.size() * sizeof(float));
    Record([n, copy](ICommandList* c) { c->SetUniformMat4Array(n.c_str(), copy.data(), copy.size() / 16); });
}
void DeferredCommandListGL::SetUniformHandle64(const char* name, uint64_t handle) {
    std::string n(name ? name : "");
    Record([n, handle](ICommandList* c) { c->SetUniformHandle64(n.c_str(), handle); });
}
void DeferredCommandListGL::SetSubroutineIndices(ShaderStage stage, uint32_t count, const uint32_t* indices) {
    std::vector<uint32_t> copy;
    if (indices && count)
        copy.assign(indices, indices + count);
    Record([stage, copy](ICommandList* c) { c->SetSubroutineIndices(stage, static_cast<uint32_t>(copy.size()), copy.empty() ? nullptr : copy.data()); });
}

void DeferredCommandListGL::Draw(uint32_t vertexCount, uint32_t firstVertex) {
    Record([vertexCount, firstVertex](ICommandList* c) { c->Draw(vertexCount, firstVertex); });
}
void DeferredCommandListGL::DrawIndexed(uint32_t indexCount, uint32_t firstIndex, int32_t vertexOffset) {
    Record([indexCount, firstIndex, vertexOffset](ICommandList* c) { c->DrawIndexed(indexCount, firstIndex, vertexOffset); });
}
void DeferredCommandListGL::DrawIndexedSubmesh(uint32_t indexCount, uint32_t firstIndex, int32_t vertexOffset) {
    Record([indexCount, firstIndex, vertexOffset](ICommandList* c) { c->DrawIndexedSubmesh(indexCount, firstIndex, vertexOffset); });
}
void DeferredCommandListGL::DrawIndexedInstanced(uint32_t indexCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t instanceCount) {
    Record([indexCount, firstIndex, vertexOffset, instanceCount](ICommandList* c) { c->DrawIndexedInstanced(indexCount, firstIndex, vertexOffset, instanceCount); });
}
void DeferredCommandListGL::DrawArraysIndirect(IBuffer* indirectBuffer, size_t offset) {
    Record([indirectBuffer, offset](ICommandList* c) { c->DrawArraysIndirect(indirectBuffer, offset); });
}
void DeferredCommandListGL::DrawIndexedIndirect(IBuffer* indirectBuffer, size_t offset) {
    Record([indirectBuffer, offset](ICommandList* c) { c->DrawIndexedIndirect(indirectBuffer, offset); });
}
void DeferredCommandListGL::MultiDrawArraysIndirect(IBuffer* indirectBuffer, size_t offset, uint32_t drawCount, uint32_t stride) {
    Record([indirectBuffer, offset, drawCount, stride](ICommandList* c) {
        c->MultiDrawArraysIndirect(indirectBuffer, offset, drawCount, stride);
    });
}
void DeferredCommandListGL::MultiDrawIndexedIndirect(IBuffer* indirectBuffer, size_t offset, uint32_t drawCount, uint32_t stride) {
    Record([indirectBuffer, offset, drawCount, stride](ICommandList* c) {
        c->MultiDrawIndexedIndirect(indirectBuffer, offset, drawCount, stride);
    });
}
void DeferredCommandListGL::MultiDrawArraysIndirectCount(IBuffer* indirectBuffer, size_t indirectOffset, IBuffer* countBuffer, size_t countBufferOffset, uint32_t maxDrawCount, uint32_t stride) {
    Record([indirectBuffer, indirectOffset, countBuffer, countBufferOffset, maxDrawCount, stride](ICommandList* c) {
        c->MultiDrawArraysIndirectCount(indirectBuffer, indirectOffset, countBuffer, countBufferOffset, maxDrawCount, stride);
    });
}
void DeferredCommandListGL::MultiDrawIndexedIndirectCount(IBuffer* indirectBuffer, size_t indirectOffset, IBuffer* countBuffer, size_t countBufferOffset, uint32_t maxDrawCount, uint32_t stride) {
    Record([indirectBuffer, indirectOffset, countBuffer, countBufferOffset, maxDrawCount, stride](ICommandList* c) {
        c->MultiDrawIndexedIndirectCount(indirectBuffer, indirectOffset, countBuffer, countBufferOffset, maxDrawCount, stride);
    });
}
void DeferredCommandListGL::Dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) {
    Record([groupCountX, groupCountY, groupCountZ](ICommandList* c) { c->Dispatch(groupCountX, groupCountY, groupCountZ); });
}
void DeferredCommandListGL::DispatchIndirect(IBuffer* indirectBuffer, size_t offset) {
    Record([indirectBuffer, offset](ICommandList* c) { c->DispatchIndirect(indirectBuffer, offset); });
}

void DeferredCommandListGL::BeginQuery(IQueryPool* pool, uint32_t index) {
    Record([pool, index](ICommandList* c) { c->BeginQuery(pool, index); });
}
void DeferredCommandListGL::EndQuery(IQueryPool* pool, uint32_t index) {
    Record([pool, index](ICommandList* c) { c->EndQuery(pool, index); });
}

void DeferredCommandListGL::BindTransformFeedbackBuffer(uint32_t slot, IBuffer* buffer, size_t offset, size_t size) {
    Record([slot, buffer, offset, size](ICommandList* c) { c->BindTransformFeedbackBuffer(slot, buffer, offset, size); });
}
void DeferredCommandListGL::BeginTransformFeedback(PrimitiveTopology primitiveMode) {
    Record([primitiveMode](ICommandList* c) { c->BeginTransformFeedback(primitiveMode); });
}
void DeferredCommandListGL::EndTransformFeedback() {
    Record([](ICommandList* c) { c->EndTransformFeedback(); });
}

void DeferredCommandListGL::BeginConditionalRender(IQueryPool* pool, uint32_t index) {
    Record([pool, index](ICommandList* c) { c->BeginConditionalRender(pool, index); });
}
void DeferredCommandListGL::EndConditionalRender() {
    Record([](ICommandList* c) { c->EndConditionalRender(); });
}

IFence* DeferredCommandListGL::InsertFence() {
    // Deferred list: fence is inserted at replay time; we cannot return it at record time.
    // Caller must use fence from immediate list after Submit, or sync another way.
    Record([](ICommandList* c) { (void)c->InsertFence(); });
    return nullptr;
}

void DeferredCommandListGL::CopyBuffer(IBuffer* src, size_t srcOffset, IBuffer* dst, size_t dstOffset, size_t size) {
    Record([src, srcOffset, dst, dstOffset, size](ICommandList* c) { c->CopyBuffer(src, srcOffset, dst, dstOffset, size); });
}

} // namespace rhi
} // namespace render
} // namespace eyelib
