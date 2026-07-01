# Rendering Module: Remaining Implementation Tasks

## 1. IBL Expansion & Hardening
- [ ] **Dynamic Occlusion**: Implement SSAO/HBAO and sample `occlusionTexture` to mask sky-light leaking in interiors.
- [ ] **Local Reflection Probes**: Implement geometric projection (parallax correction) and blending between local probes.
- [ ] **Capture Pipeline**: Support real-time environment capture for dynamic lighting scenarios.

## 2. Shadows & Lighting
- [ ] **CSM (Cascaded Shadow Maps)**: Support large-scale exterior shadow coverage with stable transitions.
- [ ] **Shadow Softening**: Refine VSM/PCF filtering and bias controls to eliminate acne/peter-panning.
- [ ] **Area Lights**: Implement Rect/Tube light support in the deferred analytical path.

## 3. Geometry & Shading
- [ ] **Skeletal Animation**: Add bone matrix support to `SceneDrawInstance` and the vertex shader.
- [ ] **Transparency Pass**: Implement sorted forward rendering or OIT for alpha-blended materials.
- [ ] **Advanced BSDFs**: Support Clearcoat, Sheen, and Anisotropy models.

## 4. Post-Processing & Temporal
- [ ] **TAA (Temporal Anti-Aliasing)**: Integrate velocity-buffer generation and jittered projection for edge stability.
- [ ] **Bloom & Tone-mapping**: High-quality convolution blur and adjustable ACES/Filmic tone-mapping.
- [ ] **Exposure Control**: Implement auto-exposure (histogram-based) for high dynamic range scenes.

## 5. Performance & Scaling
- [ ] **GPU-Driven Rendering**: Move visibility and draw-indirect generation to the GPU.
- [ ] **Cluster Culling**: Fine-grained light and geometry culling for high-density scenes.
