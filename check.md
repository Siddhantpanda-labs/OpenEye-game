# OpenEye Development Check List

## Phase 3 - Culling, LOD, Instancing, Batching ✅ COMPLETE

### Implementation Status
- **Frustum Culling**: ✅ Complete
  - Frustum class with 6-plane extraction from view-projection matrix
  - AABB vs frustum testing (outside/intersect/inside states)
  - CullingSystem integrates with render pipeline
  
- **AABB Auto-Generation**: ✅ Complete
  - Mesh::CalculateBounds() computes local-space bounds
  - CullingSystem::UpdateAABBs() transforms to world-space
  - Supports hierarchical transforms
  
- **LOD Manager**: ✅ Complete
  - Distance-based LOD switching with hysteresis (prevents popping)
  - Configurable thresholds and hysteresis values
  - LODSystem updates active LOD per frame
  - Renderable component supports multiple LOD meshes
  
- **Instancing System**: ✅ Complete
  - InstancedRenderer batches by mesh/material
  - GPU instanced rendering (up to 16,384 instances/batch)
  - Shader supports both instanced and non-instanced rendering
  - Automatic batching with configurable threshold

### To Make Complete & Industry-Proof (Future Improvements)
- **Frustum Culling**:
  - [ ] Occlusion culling (hardware queries or software)
  - [ ] Hierarchical culling (octree/BVH for spatial queries)
  - [ ] Multi-threaded culling for very large scenes
  
- **AABB**:
  - [ ] OBB (Oriented Bounding Box) for rotated objects
  - [ ] Sphere bounds for faster distance checks
  - [ ] AABB tree for broad-phase collision detection
  
- **LOD**:
  - [ ] Automatic LOD mesh generation tool
  - [ ] LOD transitions with morphing/fade
  - [ ] Impostor sprites for very far objects
  - [ ] GPU-driven LOD selection
  
- **Instancing**:
  - [ ] GPU-driven instancing (compute shader culling + instancing)
  - [ ] Instance frustum culling in compute shader
  - [ ] Per-instance color/material variations
  - [ ] Static batching tool (merge meshes offline)

---

## Phase 4 - Deferred Geometry & Lighting Passes 🚧 IN PROGRESS

### Implementation Status

- **G-Buffer**: ✅ Complete (Basic)
  - GBuffer class with 4 MRT attachments (Position, Normal, Albedo, PBR Params)
  - Depth texture attachment
  - Proper texture formats (RGB16F for position/normal, RGB8 for albedo, RG8 for PBR)
  - Resize support
  
- **Geometry Pass Shader**: ✅ Complete
  - gbuffer.vert/gbuffer.frag shaders
  - Writes to all 4 G-Buffer attachments
  - Supports instanced rendering
  
- **Fullscreen Quad Renderer**: ✅ Complete
  - FullscreenQuad class for lighting/post-processing passes
  - NDC coordinate quad with UV mapping
  
- **Geometry Pass System**: ✅ Complete
  - RenderSystem::RenderGeometryPass() fills G-Buffer
  - Integrates with culling system
  - Uses G-Buffer shader
  
- **Lighting Pass System**: 🚧 Partial
  - RenderSystem::RenderLightingPass() basic implementation
  - lighting.vert/lighting.frag shaders created
  - Simple PBR lighting calculation
  - Supports directional + point lights (up to 32)
  
- **Shadow Mapping**: ✅ Complete (Basic)
  - ShadowMap class with cascaded shadow maps (CSM)
  - Configurable cascade count (default 4) and distribution
  - PCF filtering (3x3 kernel) for soft shadows
  - Automatic cascade selection based on view depth
  - Shadow pass rendering system
  
- **Depth Pre-pass**: ❌ Not Started
- **Light Management**: ❌ Not Started (hardcoded lights)

### To Make Complete & Industry-Proof (Future Improvements)

- **G-Buffer**:
  - [ ] Optimize texture formats (pack data more efficiently)
  - [ ] Add motion vectors for TAA/temporal effects
  - [ ] Add velocity buffer for motion blur
  - [ ] Support for HDR formats
  
- **Geometry Pass**:
  - [ ] Depth pre-pass option for overdraw reduction
  - [ ] Early-Z testing optimization
  - [ ] GPU-driven rendering
  
- **Lighting Pass**:
  - [ ] Clustered forward/tiled deferred lighting (for 100+ lights)
  - [ ] Light culling (frustum/sphere culling)
  - [ ] Light volumes for attenuation
  - [ ] IBL (Image-Based Lighting) support
  - [ ] Area lights
  
- **Shadow Mapping**:
  - [x] Cascaded Shadow Maps (CSM) for directional lights ✅
  - [x] PCF filtering for soft shadows ✅
  - [ ] Variance Shadow Maps (VSM) or Exponential Shadow Maps (future optimization)
  - [ ] Shadow atlas for multiple point/spot lights (currently only directional)
  - [ ] Contact hardening shadows (distance-based shadow softening)
  - [ ] Adaptive shadow map resolution per cascade
  - [ ] Shadow map pooling/reuse for dynamic lights
  
- **Integration**:
  - [ ] Light component integration with ECS
  - [ ] Light manager system
  - [ ] Dynamic light list updates
  - [ ] Shadow map resolution scaling

---

## Notes & Remarks
- Phase 3 successfully reduces draw calls via instancing
- Frustum culling significantly improves performance for large scenes
- All systems follow industry-standard patterns (ECS, component-based)
- Phase 4 deferred rendering complete with cascaded shadow mapping
- Current rendering pipeline: Shadow Pass → Geometry Pass → Lighting Pass (deferred)
- Ready for post-processing and transparency passes (Phase 5)

