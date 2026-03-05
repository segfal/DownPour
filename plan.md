DownPour is a Vulkan-based driving simulator. Currently the road renders as a flat dark grey color (textures are loaded but the shader ignores them), the sky is a simple blue gradient on a hardcoded cube, and there is no environment around the road. This plan adds:

Realistic textured asphalt road (2 miles / 3219m)
Procedural sunny sky with sun disc and atmospheric scattering
Grass terrain alongside the road with a tiling texture


Phase 1: Realistic Road (Enable PBR Textures)
The road.glb already has asphalt textures loaded into GPU memory via MaterialManager, but world.frag ignores them and uses hardcoded grey. This phase wires up texture sampling.
Step 1.1: Create Material Descriptor Set Layout
File: src/DownPour.h — Add VkDescriptorSetLayout materialDescriptorSetLayout
File: src/DownPour.cpp — Add createMaterialDescriptorSetLayout() with 3 bindings:

binding 0: base color sampler (COMBINED_IMAGE_SAMPLER, FRAGMENT_BIT)
binding 1: normal map sampler
binding 2: metallic-roughness sampler

Call from initVulkan() after createDescriptorSetLayout().
Step 1.2: Update Descriptor Set Layout Stage Flags
File: src/DownPour.cpp → createDescriptorSetLayout() (line 278)

Change stageFlags to VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT so the CameraUBO is accessible from fragment shaders (needed for sky and fog in later phases).

Step 1.3: Expand Descriptor Pool
File: src/DownPour.cpp → createDescriptorPool() (line 226)

Add COMBINED_IMAGE_SAMPLER pool size (30 descriptors for materials)
Increase maxSets to accommodate material descriptor sets

Step 1.4: Update World Pipeline Layout
File: src/DownPour.cpp → createWorldPipeline() (line 472)

Change layout from {descriptorSetLayout} to {descriptorSetLayout, materialDescriptorSetLayout}
This gives world shader: set 0 = camera UBO, set 1 = material textures

Step 1.5: Initialize MaterialManager Descriptor Support
File: src/DownPour.cpp → after creating descriptor pool and loading road model

Call materialManager->initDescriptorSupport(materialDescriptorSetLayout, descriptorPool, MAX_FRAMES_IN_FLIGHT)
Call materialManager->createDescriptorSetsForExistingMaterials()

File: src/renderer/MaterialManager.cpp

Extend descriptor writes in createDescriptorSetsForExistingMaterials() to write bindings 0, 1, 2 (baseColor, normalMap, metallicRoughness). Use default white texture as fallback for missing textures.

Step 1.6: Update world.vert
File: shaders/world.vert

Add layout(location = 2) out vec3 fragWorldPos output
Set fragWorldPos = inPosition

Step 1.7: Rewrite world.frag for Textured PBR-lite
File: shaders/world.frag

Add layout(set = 1, binding = 0) uniform sampler2D baseColorMap
Sample base color texture instead of hardcoded grey
Add sun direction lighting with warm color (1.0, 0.98, 0.92)
Add blue-tinted outdoor ambient (0.15, 0.18, 0.25)
Reinhard tone mapping

Step 1.8: Bind Material in Render Loop
File: src/DownPour.cpp → recordCommandBuffer() (line 316)

After binding camera descriptor set (set 0), also bind road material descriptor set (set 1)
Use materialManager->getDescriptorSet(roadMaterialIds[0], frameIndex)

Step 1.9: Compile Shaders
bashglslc shaders/world.vert -o shaders/world.vert.spv
glslc shaders/world.frag -o shaders/world.frag.spv

Phase 2: Realistic Sunny Sky
Replace the gradient skybox with a procedural Preetham-inspired atmospheric sky with sun disc.
Step 2.1: Extend CameraUBO
File: src/DownPour.h
cppstruct CameraUBO {
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;
    alignas(16) glm::mat4 viewProj;
    alignas(16) glm::vec4 sunDirection;    // xyz = dir, w = intensity
    alignas(16) glm::vec4 cameraPosition;  // xyz = pos, w = time
};
File: src/DownPour.cpp → updateUniformBuffer()

Populate sunDirection = (0.4, 0.8, 0.3, 1.0) normalized
Populate cameraPosition from camera entity

Step 2.2: Add depthCompareOp to PipelineConfig
File: src/core/PipelineFactory.h — Add VkCompareOp depthCompareOp = VK_COMPARE_OP_LESS to PipelineConfig
File: src/core/PipelineFactory.cpp — Use config.depthCompareOp in depth stencil state
Step 2.3: Update Skybox Pipeline Config
File: src/DownPour.cpp → createGraphicsPipeline()

Set config.enableDepthWrite = false
Set config.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL

Step 2.4: Rewrite basic.vert
File: shaders/basic.vert

Access CameraUBO (now with sunDirection, cameraPosition)
Strip translation from view matrix: mat4 viewNoTranslation = mat4(mat3(camera.view))
Set gl_Position = clipPos.xyww so skybox depth = 1.0 (always behind)
Output fragDirection = pos for sky direction lookup

Step 2.5: Rewrite basic.frag — Procedural Sky
File: shaders/basic.frag

Access CameraUBO for sun direction
Preetham-inspired gradient: deep blue zenith → lighter horizon
Rayleigh scattering glow near horizon
Sun disc with smoothstep(0.9995, 0.9999, sunDot) and corona glow
Below-horizon ground fade
Reinhard tone mapping

Step 2.6: Change Draw Order
File: src/DownPour.cpp → recordCommandBuffer()

Draw road FIRST (writes depth), then terrain, then skybox LAST (fills remaining pixels at depth=1.0)

Step 2.7: Compile Shaders
bashglslc shaders/basic.vert -o shaders/basic.vert.spv
glslc shaders/basic.frag -o shaders/basic.frag.spv

Phase 3: Grass Terrain
Add flat grass terrain on both sides of the road with a tiling grass texture and distance fog.
Step 3.1: Source Grass Texture

Download a CC0 seamless grass texture (e.g., from Poly Haven)
Save as assets/textures/grass/grass_diff.jpg

Step 3.2: Create TerrainGeometry
New file: src/renderer/TerrainGeometry.h
New file: src/renderer/TerrainGeometry.cpp

generate(roadWidth, terrainWidth, terrainLength, texTileSize) — creates two rectangular strips flanking the road

Left strip: x from -(roadWidth/2 + terrainWidth) to -roadWidth/2
Right strip: x from roadWidth/2 to roadWidth/2 + terrainWidth
z from 0 to terrainLength (3300m), y = 0
Subdivided grid (every 10m) for proper UV tiling
Normals all (0, 1, 0)


createBuffers() — upload to Vulkan vertex/index buffers via ResourceManager
cleanup() — destroy GPU resources

Step 3.3: Create Terrain Shaders
New file: shaders/terrain.vert — Same structure as world.vert, pass through position/normal/texcoord/worldPos + distance to camera
New file: shaders/terrain.frag

Sample tiling grass texture from set 1, binding 0
Sun-consistent lighting (same sunDirection from CameraUBO)
Distance fog: blend toward sky horizon color (0.55, 0.70, 0.90) from 200m to 2000m

Step 3.4: Create Terrain Pipeline
File: src/DownPour.h — Add terrainPipeline, terrainPipelineLayout, terrainGeometry, grassMaterialId
File: src/DownPour.cpp

createTerrainPipeline() — uses {descriptorSetLayout, materialDescriptorSetLayout}, terrain shaders, cull NONE
createTerrain() — generates geometry, loads grass texture via MaterialManager

Step 3.5: Draw Terrain
File: src/DownPour.cpp → recordCommandBuffer()

Draw terrain after road, before skybox
Bind camera descriptor set (set 0) + grass material descriptor set (set 1)

Step 3.6: Add Distance Fog to Road Shader
File: shaders/world.frag

Add fog calculation matching terrain shader for visual consistency
Blend toward horizon color at distance

Step 3.7: Update Build
File: CMakeLists.txt — Add src/renderer/TerrainGeometry.cpp
Step 3.8: Cleanup
File: src/DownPour.cpp → cleanup()

Destroy terrain geometry, terrain pipeline, terrain pipeline layout, material descriptor set layout

Step 3.9: Compile Shaders
bashglslc shaders/terrain.vert -o shaders/terrain.vert.spv
glslc shaders/terrain.frag -o shaders/terrain.frag.spv

Files Summary
Modified (existing)
FileChangessrc/DownPour.hmaterialDescriptorSetLayout, CameraUBO extension, terrain memberssrc/DownPour.cppMaterial DSL, expanded pool, pipeline layouts, terrain init, draw order, cleanupsrc/core/PipelineFactory.hAdd depthCompareOp to PipelineConfigsrc/core/PipelineFactory.cppUse configurable depthCompareOpsrc/renderer/MaterialManager.cppMulti-binding descriptor writes (baseColor + normal + roughness)shaders/basic.vertView translation strip, depth=w trick, direction outputshaders/basic.fragProcedural Preetham sky with sunshaders/world.vertAdd fragWorldPos outputshaders/world.fragTexture sampling, PBR-lite lighting, fogCMakeLists.txtAdd TerrainGeometry.cpp
New files
FilePurposesrc/renderer/TerrainGeometry.hProcedural terrain mesh generator headersrc/renderer/TerrainGeometry.cppTerrain mesh generation + GPU uploadshaders/terrain.vertTerrain vertex shadershaders/terrain.fragTerrain fragment shader with grass texture + fogassets/textures/grass/grass_diff.jpgCC0 seamless grass texture (sourced externally)

Verification

Build: ./run.sh (or cmake + make) — should compile without errors
Road test: Launch app, look at road — should show asphalt texture with sunlit PBR-lite lighting instead of flat grey
Sky test: Look up — should see blue sky gradient with visible sun disc and corona glow, no seams
Grass test: Look to the sides — should see green grass terrain extending alongside the road
Fog test: Look far down the road — road and grass should fade into a haze matching the sky horizon color
Camera test: Fly along the 2-mile road with WASD — all elements should render consistently at distance
Performance: Should maintain 60fps on M3 — we're only adding ~2 extra draw calls (terrain + skybox reorder)