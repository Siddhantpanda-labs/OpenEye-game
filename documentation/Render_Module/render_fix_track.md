# shadow fix

### Comparison with Aespinosa_vulkan Implementation

**Our Implementation (OpenEye Render Module)**
- **Architecture**: We implement Cascaded Shadow Maps (CSM) specifically for directional lights, using orthographic projections sliced along the camera's Z-axis (view-space depth).
- **Stabilization**: We have sub-texel camera stabilization (`roundOffset`) which prevents shimmering and crawling artifacts along shadow edges when the camera moves.
- **Bias**: We handle both normal bias and depth bias directly in the shader, combined dynamically based on the N dot L angle, which significantly helps in preventing Peter Panning and shadow acne.
- **Filtering**: We feature cascade split blending to ensure smooth transitions between shadow LOD levels. 

**Aespinosa_vulkan Implementation**
- **Architecture**: Aespinosa uses standard perspective projection matrices for all light types (treating directional lights like a large spot light). It does not implement Cascaded Shadow Maps (CSM).
- **Advanced Features**: It implements Variance Shadow Maps (VSM) and hardware Raytraced Shadows, which gives much softer and physically accurate shadows out of the box compared to our basic PCF approach.
- **Bias**: Aespinosa utilizes Vulkan's hardware depth bias capabilities (`vkCmdSetDepthBias`) during the rasterization phase. This is often more performant and handles slope-scaled depth bias natively on the GPU hardware, whereas our bias is computed manually in the fragment shader.

**Where Our Implementation is Weak**
1. **Lack of Hardware Depth Bias**: We rely entirely on shader-side manual bias math. Incorporating Vulkan's `vkCmdSetDepthBias` in the pipeline would improve performance and provide better slope-scaled bias, reducing acne without pushing the bias values too high (which causes Peter Panning).
2. **Missing Advanced Filtering (VSM/PCSS)**: Our filtering relies on a basic uniform PCF kernel. Aespinosa implements Variance Shadow Maps (VSM) which provide superior, artifact-free soft shadows with better performance for large blur radii.
3. **No Spot/Point Light Shadows**: Currently, our system only handles directional light cascades (P7.3). Aespinosa seamlessly handles spot and point lights via a unified `sampler2DArray` and `ShadowType` structure. We have this planned for P7.4 and P7.5, but it is currently a missing feature.
4. **Hardcoded Limits**: Our shadow map resolution is currently hardcoded (`kShadowMapResolution = 2048.0f`), which prevents dynamic quality scaling compared to more flexible systems.
