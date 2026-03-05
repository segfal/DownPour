# Road Rendering System — Code Trace

Complete trace of how the road is loaded, transformed, shaded (including full GLSL breakdown), and rendered each frame.

---

## 1. Initialization Order

**Entry:** [`src/DownPour.cpp:39-89`](src/DownPour.cpp#L39) — `initVulkan()`

```
initVulkan()
  ├─ createDescriptorSetLayout()          ← camera UBO layout (set 0)
  ├─ createMaterialDescriptorSetLayout()  ← material textures layout (set 1)
  ├─ createWorldPipeline()                ← world.vert + world.frag
  ├─ createCarPipeline()                  ← car.vert + car.frag (NOT used for road)
  ├─ ...
  ├─ loadRoadModel()                      ← src/DownPour.cpp:755
  ├─ loadCarModel()
  ├─ initCamera()
  └─ createSyncObjects()
```

---

## 2. Road Model Loading

**Function:** [`src/DownPour.cpp:755-791`](src/DownPour.cpp#L755) — `loadRoadModel()`

### 2.1 Load GLB File

```cpp
roadAdapter->load("assets/models/road.glb", ...)    ← DownPour.cpp:757
```

**ModelAdapter::load()** at [`src/renderer/ModelAdapter.cpp:21-42`](src/renderer/ModelAdapter.cpp#L21):
```
1. Create Model instance                              ← ModelAdapter.cpp:26
2. model->loadFromFile("assets/models/road.glb")      ← ModelAdapter.cpp:28
   └─ GLTFLoader::load()                              ← GLTFLoader.cpp:19
      ├─ tinygltf parses GLB binary
      ├─ Extract vertices (position, normal, texCoord) ← GLTFLoader.cpp:91-126
      ├─ Extract indices (16-bit or 32-bit)            ← GLTFLoader.cpp:129-145
      ├─ Extract materials + embedded textures         ← GLTFLoader.cpp:171-223
      └─ Extract node hierarchy + AABB bounds          ← GLTFLoader.cpp:242-306
3. loadMetadata("assets/models/road.glb.json")        ← ModelAdapter.cpp:34
   └─ File NOT found — road has no sidecar JSON
```

**No sidecar JSON exists for road.** All road configuration is hardcoded in `loadRoadModel()`.

### 2.2 Vertex Data

**GLTFLoader** extracts vertices in **mesh-local space** (no node transforms applied):

[`src/renderer/GLTFLoader.cpp:91-126`](src/renderer/GLTFLoader.cpp#L91):
```cpp
for (size_t i = 0; i < posAccessor.count; ++i) {
    vertex.position = vec3(pos[0], pos[1], pos[2]);    // Raw from GLB buffer
    vertex.normal   = vec3(norm[0], norm[1], norm[2]);  // Surface normal
    vertex.texCoord = vec2(texCoord[0], texCoord[1]);   // UV for texture sampling
    outModel.vertices.push_back(vertex);
}
```

Each vertex is 32 bytes:
```
| Offset | Size | Field    | Type |
|--------|------|----------|------|
| 0      | 12   | position | vec3 |
| 12     | 12   | normal   | vec3 |
| 24     | 8    | texCoord | vec2 |
```

### 2.3 Road Transform (Model Matrix)

[`src/DownPour.cpp:774-777`](src/DownPour.cpp#L774):
```cpp
glm::mat4 roadTransform = glm::mat4(1.0f);
roadTransform = glm::translate(roadTransform, glm::vec3(0.0f, 0.0f, 0.0f));  // Origin
roadTransform = glm::scale(roadTransform, glm::vec3(3000.0f, 1000.0f, 1000.0f));
roadModelPtr->setModelMatrix(roadTransform);
```

**Scale breakdown:**
- X: ×3000 — road is 3× wider than the base mesh (to create a wide highway)
- Y: ×1000 — standard 1000× environment scale
- Z: ×1000 — standard 1000× environment scale (road extends along Z)

This model matrix is pushed to the vertex shader each frame as a push constant.

### 2.4 Road Materials
zr
[`src/DownPour.cpp:780-784`](src/DownPour.cpp#L780):
```cpp
const auto& roadMaterials = roadModelPtr->getMaterials();
for (size_t i = 0; i < roadMaterials.size(); i++) {
    uint32_t gpuId     = materialManager->createMaterial(roadMaterials[i]);
    roadMaterialIds[i] = gpuId;
}
```

**MaterialManager::createMaterial()** at [`src/renderer/MaterialManager.cpp:29-147`](src/renderer/MaterialManager.cpp#L29):

For each material, creates GPU resources:
1. Load **baseColor** texture (embedded in GLB or default white 1×1)
2. Load **normalMap** texture (embedded or default white)
3. Load **metallicRoughness** texture (embedded or default white)
4. Allocate descriptor sets (one per frame, MAX_FRAMES_IN_FLIGHT = 2)
5. Bind textures to descriptor sets:
   - **Binding 0**: baseColorMap
   - **Binding 1**: normalMap
   - **Binding 2**: metallicRoughnessMap

**Default white texture:** If a map is missing, a 1×1 white pixel is used ([`MaterialManager.cpp:331-348`](src/renderer/MaterialManager.cpp#L331)). In the fragment shader, white base color = neutral, white normal = `(1,1,1)` normalized = default up, white metallic-roughness = `(1,1)` = fully rough + non-metallic.

### 2.5 RoadEntity (Minimal)

[`src/scene/RoadEntity.h:1-31`](src/scene/RoadEntity.h#L1):
```cpp
class RoadEntity : public Entity {
    struct Config {
        float    width    = 10.0f;       // meters
        float    length   = 50000.0f;    // ~50km
        uint32_t segments = 1000;
    };
};
```

Created at [`DownPour.cpp:788`](src/DownPour.cpp#L788) but **not used for rendering**. The road is drawn via a legacy flat loop in `recordCommandBuffer()`, not through the scene graph. RoadEntity exists as a placeholder for future scene-graph integration.

---

## 3. World Pipeline (Road Shader Pipeline)

### 3.1 Pipeline Creation

**Function:** [`src/DownPour.cpp:721-740`](src/DownPour.cpp#L721) — `createWorldPipeline()`

```cpp
PipelineConfig config;
config.vertShader = "world.vert.spv";
config.fragShader = "world.frag.spv";
config.layout     = worldPipelineLayout;
config.cullMode   = VK_CULL_MODE_NONE;       // Both sides rendered
// enableBlending  = false  (default)         // Opaque
// enableDepthWrite = true  (default)         // Writes depth buffer
// depthCompareOp   = VK_COMPARE_OP_LESS      // Standard Z-buffer
```

### 3.2 Pipeline Layout

[`src/DownPour.cpp:726-727`](src/DownPour.cpp#L726):
```
worldPipelineLayout:
  ├─ Set 0: Camera UBO
  │   └─ Binding 0: CameraUBO (view, proj, viewProj, sunDir, camPos, weather)
  ├─ Set 1: Material Textures
  │   ├─ Binding 0: baseColorMap          (sampler2D)
  │   ├─ Binding 1: normalMap             (sampler2D)
  │   └─ Binding 2: metallicRoughnessMap  (sampler2D)
  └─ Push Constant: mat4 model            (64 bytes, vertex stage)
```

### 3.3 Pipeline Factory Details

[`src/core/PipelineFactory.cpp:11-151`](src/core/PipelineFactory.cpp#L11):

| Setting | Value | Line |
|---------|-------|------|
| Vertex stride | 32 bytes | PipelineFactory.cpp:38 |
| Vertex attributes | pos(vec3), norm(vec3), uv(vec2) | :41-53 |
| Polygon mode | FILL | :83 |
| Front face | COUNTER_CLOCKWISE | :85 |
| Cull mode | NONE (from config) | :86 |
| Depth test | ENABLED | :97 |
| Depth write | ENABLED | :98 |
| Depth compare | LESS | :99 |
| Blending | DISABLED | :103 |
| Color write mask | RGBA | :107 |

---

## 4. Camera UBO — Shared Data for Road Shader

**Function:** [`src/DownPour.cpp:195-239`](src/DownPour.cpp#L195) — `updateUniformBuffer()`

Updated every frame before rendering:

```cpp
struct CameraUBO {
    mat4 view;           // Camera view matrix                 ← :198
    mat4 proj;           // Projection matrix (Y-flipped)      ← :199, :207
    mat4 viewProj;       // proj × view                        ← :208
    vec4 sunDirection;   // xyz = (0.4, 0.8, 0.3), w = 5.0    ← :213-214
    vec4 cameraPosition; // xyz = world pos, w = elapsed time  ← :223-224
    vec4 weatherParams;  // x=rain, y=wet, z=windX, w=windZ   ← :231-236
};
```

**Key values for road rendering:**

| UBO Field | Value | Used By |
|-----------|-------|---------|
| `sunDirection.xyz` | `normalize(0.4, 0.8, 0.3)` | PBR light direction |
| `sunDirection.w` | `5.0` | HDR sun intensity (compensates PI division in PBR) |
| `cameraPosition.w` | `glfwGetTime()` | Splash ripple animation timer |
| `weatherParams.x` | `0.0–1.0` | Rain intensity (fog density, splash ripples, sun dimming) |
| `weatherParams.y` | `0.0–1.0` | Wetness (surface darkening + glossiness) |

---

## 5. Road Rendering — Per-Frame Draw

**Function:** [`src/DownPour.cpp:432-460`](src/DownPour.cpp#L432) — road section of `recordCommandBuffer()`

```
Road is drawn FIRST in the render order (writes depth for all other geometry).

Step-by-step:
  1. Guard: roadModelPtr exists, has indices, has materials       ← :434
  2. Bind worldPipeline (world.vert + world.frag)                ← :439
  3. Bind camera UBO descriptor set (set 0)                      ← :442-443
  4. Push road model matrix (scale 3000×1000×1000)               ← :446-448
  5. Bind road material descriptor set (set 1)                   ← :451-455
     └─ Textures: baseColor, normalMap, metallicRoughness
  6. Bind road vertex + index buffers                            ← :457-458
  7. Draw indexed (all road indices in one call)                 ← :459
```

**Single draw call:** The road is rendered as one flat mesh with one material, no per-node scene graph traversal. This differs from the car, which uses per-node rendering via `getRenderBatches()`.

### 5.1 Full Render Order

| Order | What | Pipeline | Depth |
|-------|------|----------|-------|
| **1** | **Road** | `worldPipeline` | **Writes depth** |
| 1.5 | Car (per-node) | `carPipeline` | Writes depth |
| 2 | Terrain (grass) | `terrainPipeline` | Writes depth |
| 3 | Rain particles | `rainPipeline` | Transparent |
| 4 | Skybox | `graphicsPipeline` | depth ≤ 1.0, no write |
| 5 | Screen rain overlay | `screenRainPipeline` | Ignores depth |

---

## 6. Vertex Shader — `world.vert`

**File:** [`shaders/world.vert`](shaders/world.vert) (38 lines)

### 6.1 Inputs

```glsl
// Descriptor Set 0: Camera
layout(set = 0, binding = 0) uniform CameraUBO {
    mat4 view, projection, viewProjection;
    vec4 sunDirection;     // xyz = dir, w = intensity
    vec4 cameraPosition;   // xyz = pos, w = time
    vec4 weatherParams;    // x = rain, y = wet, z/w = wind
} camera;

// Push Constant: per-object model matrix
layout(push_constant) uniform PushConstants {
    mat4 model;            // Road: scale(3000, 1000, 1000)
} push;

// Vertex attributes from GLB
layout(location = 0) in vec3 inPosition;    // Mesh-local position
layout(location = 1) in vec3 inNormal;      // Mesh-local normal
layout(location = 2) in vec2 inTexCoord;    // UV coordinates
```

### 6.2 Processing

[`shaders/world.vert:25-38`](shaders/world.vert#L25):

```glsl
void main() {
    // 1. Transform vertex to world space
    vec4 worldPos = push.model * vec4(inPosition, 1.0);          // :26
    gl_Position = camera.viewProjection * worldPos;              // :27

    // 2. Transform normal (handles non-uniform scale like 3000×1000×1000)
    mat3 normalMatrix = transpose(inverse(mat3(push.model)));    // :30
    fragNormal = normalize(normalMatrix * inNormal);             // :31

    // 3. Pass-through
    fragTexCoord = inTexCoord;                                   // :33
    fragWorldPos = worldPos.xyz;                                 // :34

    // 4. Distance for fog
    fragDistance = length(camera.cameraPosition.xyz - worldPos.xyz);  // :37
}
```

**Why `transpose(inverse(mat3(...)))`?**
The road has non-uniform scale (3000, 1000, 1000). Regular `mat3(model) * normal` would skew the normals — a flat road normal `(0,1,0)` would lean toward the X axis. The inverse-transpose corrects for this, keeping normals perpendicular to surfaces.

### 6.3 Outputs → Fragment Shader

```glsl
layout(location = 0) out vec3 fragNormal;     // World-space surface normal
layout(location = 1) out vec2 fragTexCoord;   // UV for texture sampling
layout(location = 2) out vec3 fragWorldPos;   // World-space position (used for road markings, splash)
layout(location = 3) out float fragDistance;  // Camera-to-fragment distance (used for fog)
```

---

## 7. Fragment Shader — `world.frag`

**File:** [`shaders/world.frag`](shaders/world.frag) (365 lines)

This is the most complex shader in the project. It handles PBR rendering, procedural road markings, wet surface effects, animated splash ripples, fog, and tone mapping — all in one pass.

### 7.1 Shader Execution Pipeline

```
fragTexCoord, fragNormal, fragWorldPos, fragDistance
           │
           ▼
┌──────────────────────────────────┐
│ 1. Stochastic UV Sampling        │  :258-264   Anti-tiling for large surfaces
│    ├─ baseColor                  │
│    ├─ metallicRoughness          │
│    └─ normalMap                  │
├──────────────────────────────────┤
│ 2. Normal Mapping                │  :269-278   Cotangent-frame TBN
├──────────────────────────────────┤
│ 3. Procedural Road Markings      │  :283-289   Yellow center, white lanes
├──────────────────────────────────┤
│ 3b. Wet Surface Effects          │  :296-305   Darken + smooth + splash ripples
├──────────────────────────────────┤
│ 4. Cook-Torrance PBR             │  :310-337   GGX/Smith/Fresnel BRDF
├──────────────────────────────────┤
│ 5. Ambient Lighting              │  :342-345   Overcast-adjusted ambient
├──────────────────────────────────┤
│ 6. Exponential Fog               │  :350-353   Rain-density-modulated
├──────────────────────────────────┤
│ 7. Reinhard Tone Mapping         │  :358       HDR → LDR
└──────────────────────────────────┘
           │
           ▼
     outColor = vec4(color, 1.0)
```

---

### 7.2 Stage 1: Stochastic Texture Sampling (Anti-Tiling)

**Problem:** The road surface uses a tiled asphalt texture. At any reasonable texture resolution, the tiling pattern becomes visible as repeating squares over large distances.

**Solution:** Inigo Quilez's hash-offset technique. Sample the texture 4 times from neighboring tile cells with random UV offsets and mirroring, then blend smoothly at tile boundaries.

#### `computeStochasticUV()` — [`world.frag:117-142`](shaders/world.frag#L117)

```glsl
StochasticUV computeStochasticUV(vec2 uv) {
    vec2 iuv = floor(uv);             // Integer tile coordinate
    vec2 fuv = fract(uv);             // Fractional position within tile

    // 4-component hashes for each of the 4 neighboring tiles
    // .xy = random UV offset, .zw = random mirror direction (±1)
    vec4 ofa = hash4(iuv + vec2(0,0));  // Bottom-left tile    :122
    vec4 ofb = hash4(iuv + vec2(1,0));  // Bottom-right tile   :123
    vec4 ofc = hash4(iuv + vec2(0,1));  // Top-left tile       :124
    vec4 ofd = hash4(iuv + vec2(1,1));  // Top-right tile      :125

    // Mirror direction: ±1 for each tile
    ofa.zw = sign(ofa.zw - 0.5);       // :130-133

    // Final UVs: original UV × mirror ± random offset
    s.uva = uv * ofa.zw + ofa.xy;      // :135-138

    // Smooth blend between tiles (avoid hard edges)
    s.blend = smoothstep(0.25, 0.75, fuv);  // :140
}
```

**`textureGrad()` instead of `texture()`:** Uses explicit screen-space derivatives (`dFdx`/`dFdy`) to keep mipmapping correct despite the modified UV coordinates. Without this, mipmapping would break at tile boundaries.

#### `textureNoTile3()` — [`world.frag:144-152`](shaders/world.frag#L144)

Bilinear blend of 4 hash-offset texture samples:
```glsl
vec3 textureNoTile3(sampler2D samp, StochasticUV s) {
    return mix(
        mix(sample_A, sample_B, blend.x),    // Blend along X
        mix(sample_C, sample_D, blend.x),
        blend.y                               // Blend along Y
    );
}
```

**`textureNoTile2()`** — same but returns `.gb` channels (for metallic-roughness).

**Performance note:** 4 texture samples per pixel per map = 12 total texture samples (3 maps × 4 samples). This is expensive but necessary for large surfaces.

---

### 7.3 Stage 2: Normal Mapping

[`world.frag:269-278`](shaders/world.frag#L269):

```glsl
vec3 N = normalize(fragNormal);                               // :269

vec3 normalSample = textureNoTile3(normalMap, suv);           // :271
bool isDefaultNormalMap = all(greaterThan(normalSample, vec3(0.95)));  // :272

if (!isDefaultNormalMap) {
    vec3 tangentNormal = normalSample * 2.0 - 1.0;           // [0,1] → [-1,1]
    mat3 TBN = cotangentFrame(N, fragWorldPos, fragTexCoord); // :276
    N = normalize(TBN * tangentNormal);                       // :277
}
```

**Default detection:** If the normal map is the 1×1 white fallback, all channels > 0.95. Skipping TBN computation saves GPU cycles.

#### `cotangentFrame()` — [`world.frag:71-85`](shaders/world.frag#L71)

Constructs a tangent-bitangent-normal matrix from screen-space partial derivatives:
```glsl
mat3 cotangentFrame(vec3 N, vec3 p, vec2 uv) {
    vec3 dp1  = dFdx(p);       // World-space position change per screen pixel (X)
    vec3 dp2  = dFdy(p);       // World-space position change per screen pixel (Y)
    vec2 duv1 = dFdx(uv);     // UV change per screen pixel (X)
    vec2 duv2 = dFdy(uv);     // UV change per screen pixel (Y)

    // Compute tangent and bitangent from position/UV derivatives
    vec3 T = dp2perp * duv1.x + dp1perp * duv2.x;
    vec3 B = dp2perp * duv1.y + dp1perp * duv2.y;

    return mat3(T * invmax, B * invmax, N);
}
```

**Why cotangent frame?** The road model doesn't store tangent vectors. This method derives them per-pixel from screen-space derivatives, avoiding the need for precomputed tangents in the vertex data.

---

### 7.4 Stage 3: Procedural Road Markings

**Function:** [`world.frag:207-243`](shaders/world.frag#L207) — `roadMarkings()`

All lane markings are generated procedurally in world-space coordinates. No marking textures needed.

```
Road layout (cross-section at any Z):

  -7.4m      -3.7m       0       +3.7m      +7.4m
    │          │          │          │          │
    ┃ shoulder ┆ lane     ┆ center   ┆ lane     ┃ shoulder
    ┃ (solid   ┆ (dashed  ┆ (dashed  ┆ (dashed  ┃ (solid
    ┃  white)  ┆  white)  ┆  yellow) ┆  white)  ┃  white)
```

#### Center Line — Yellow Dashed

[`world.frag:217-224`](shaders/world.frag#L217):
```glsl
float centerHalfWidth = 0.06;      // 12cm total width
float centerEdge = smoothstep(..., abs(x));    // X distance from center
float dashPhase = mod(z, 8.0);                 // 8m cycle: 3m dash + 5m gap
float dashMask = smoothstep(..., dashPhase);   // Dash pattern
float centerMark = centerEdge * dashMask;
vec3 yellowColor = vec3(0.95, 0.8, 0.1);
```

#### Lane Edge Lines — White Dashed

[`world.frag:226-229`](shaders/world.frag#L226):
```glsl
float laneHalfWidth = 0.05;        // 10cm total width
float laneEdge = smoothstep(..., abs(abs(x) - 3.7));   // X at ±3.7m
float laneMark = laneEdge * dashMask;                   // Same dash pattern
```

#### Shoulder Lines — White Solid

[`world.frag:232-234`](shaders/world.frag#L232):
```glsl
float shoulderHalfWidth = 0.075;   // 15cm total width
float shoulderMark = smoothstep(..., abs(abs(x) - 7.4));  // X at ±7.4m
// No dashMask — solid continuous line
```

**Anti-aliasing:** Uses `fwidth(x)` and `fwidth(z)` for screen-space derivatives in `smoothstep()`, producing smooth edges regardless of camera distance.

#### Blending into Base Color

[`world.frag:287-289`](shaders/world.frag#L287):
```glsl
baseColor = mix(baseColor, markingColor, markingBlend);   // Paint over asphalt
roughness = mix(roughness, 0.35, markingBlend);           // Paint is smoother than asphalt
metallic  = mix(metallic, 0.0, markingBlend);             // Paint is non-metallic
```

---

### 7.5 Stage 3b: Wet Surface Effects

[`world.frag:296-305`](shaders/world.frag#L296):

**Wet asphalt darkening** — water fills micro-cavities, trapping light:
```glsl
baseColor *= mix(1.0, 0.55, wetness);    // :296  Up to 45% darker when fully wet
roughness  = mix(roughness, 0.08, wetness);  // :297  Glossy water film
```

**Splash ripple normal perturbation** — animated rain impact rings:
```glsl
float splash = splashRipples(fragWorldPos.xz, elapsedTime, rainIntensity);  // :300
if (splash > 0.01) {
    float splashBump = splash * 0.15;
    N = normalize(N + vec3(splashBump, 0.0, splashBump));  // :304
}
```

#### `splashRipples()` — [`world.frag:167-202`](shaders/world.frag#L167)

Generates animated expanding rings across the road surface:

```
3-layer system (each layer at different scale):

  Layer 0: scale 1.5  — large ripples
  Layer 1: scale 2.2  — medium ripples
  Layer 2: scale 2.9  — small ripples

Per-layer per-cell:
  1. Hash cell coordinates → random splash center (0-1, 0-1)
  2. Hash cell → random cycle period (0.6s – 1.0s)
  3. Compute age = phase / cycle  (0 = just spawned, 1 = faded)
  4. Ring radius = age × 0.4 (expands outward)
  5. Ring shape = smoothstep band around radius (width 0.04)
  6. Fade = (1 - age)²  (quadratic decay)
  7. Sum all layers, clamp to [0, 1] × rainIntensity
```

**Result:** Normal perturbation creates specular highlights that look like rain impacts rippling across the wet road.

---

### 7.6 Stage 4: Cook-Torrance PBR BRDF

[`world.frag:310-337`](shaders/world.frag#L310):

**PBR theory:** Physically-Based Rendering models how light interacts with surfaces. For the road, this means realistic specular reflections on wet asphalt, correct energy conservation between diffuse and specular, and metallic handling for markings.

#### Setup Vectors

```glsl
vec3 V = normalize(cameraPosition - fragWorldPos);  // View direction      :310
vec3 L = normalize(sunDirection);                    // Light direction     :311
vec3 H = normalize(V + L);                          // Half-vector         :312

float NdotL = max(dot(N, L), 0.0);    // Surface facing light?             :314
float NdotV = max(dot(N, V), 0.001);  // Surface facing camera?            :315
float NdotH = max(dot(N, H), 0.0);    // Normal aligned with half-vector?  :316
float HdotV = max(dot(H, V), 0.0);    // For Fresnel term                  :317
```

#### F0 (Surface Reflectance at Normal Incidence)

```glsl
vec3 F0 = mix(vec3(0.04), baseColor, metallic);    // :319
```
- Non-metallic (asphalt): F0 = 0.04 (4% reflectance — standard dielectric)
- Metallic: F0 = baseColor (metal reflects its own color)

#### Specular BRDF Components

| Component | Function | Line | Formula |
|-----------|----------|------|---------|
| **D** (Normal Distribution) | `distributionGGX()` | :38-43 | `α² / (π × (NdotH² × (α²-1) + 1)²)` |
| **G** (Geometry/Shadowing) | `geometrySmith()` | :55-59 | `G₁(NdotV) × G₁(NdotL)` using Schlick-GGX |
| **F** (Fresnel) | `fresnelSchlick()` | :64-66 | `F0 + (1-F0) × (1-cosθ)⁵` |

```glsl
vec3 specular = (D * G * F) / (4.0 * NdotV * NdotL + 0.0001);  // :325
```

#### Energy Conservation

```glsl
vec3 kS = F;                                         // Specular fraction = Fresnel
vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);      // Diffuse = remaining energy
vec3 diffuse = kD * baseColor / PI;                  // Lambertian diffuse
```

#### Final Lighting

```glsl
vec3 sunColor = vec3(1.0, 0.98, 0.92);                              // :332
float sunIntensity = max(sunDirection.w, 1.0);                       // :333 = 5.0
sunIntensity *= mix(1.0, 0.35, rainIntensity);                       // :334 dim in rain
vec3 sunRadiance = sunColor * sunIntensity;                          // :335

vec3 Lo = (diffuse + specular) * sunRadiance * NdotL;               // :337
```

---

### 7.7 Stage 5: Ambient Lighting

[`world.frag:342-345`](shaders/world.frag#L342):

```glsl
vec3 ambientBase = mix(vec3(0.22, 0.24, 0.30),       // Clear sky: cool blue-gray
                       vec3(0.25, 0.26, 0.28),        // Rainy: neutral gray
                       rainIntensity);
vec3 ambient = ambientBase * baseColor;
vec3 color = ambient + Lo;
```

Ambient increases slightly in rain to simulate overcast sky scattering more light from all directions.

---

### 7.8 Stage 6: Exponential Fog

[`world.frag:350-353`](shaders/world.frag#L350):

```glsl
float fogDensity = mix(0.0000002, 0.0000012, rainIntensity);  // :350
float fogFactor = 1.0 - exp(-fogDensity * fragDistance);       // :351
vec3 fogColor = mix(vec3(0.75, 0.85, 1.0),                    // Clear: pale blue
                    vec3(0.55, 0.58, 0.62),                    // Rain: gray
                    rainIntensity);                             // :352
color = mix(color, fogColor, fogFactor);                       // :353
```

**Fog density is tiny** because the environment is 1000× scaled. At 1000 world units distance (= 1 real meter), `fogFactor ≈ 0.0002` (invisible). At 100,000 units (100m), `fogFactor ≈ 0.02` (barely visible). Fog becomes apparent at extreme distances (millions of units) or in heavy rain.

---

### 7.9 Stage 7: Tone Mapping

[`world.frag:358`](shaders/world.frag#L358):

```glsl
color = color / (color + vec3(1.0));   // Reinhard tone mapping
```

Maps HDR values (sunIntensity = 5.0 can produce values > 1.0) to LDR [0, 1] range. Reinhard compresses highlights while preserving dark detail.

---

### 7.10 Output

[`world.frag:363`](shaders/world.frag#L363):
```glsl
outColor = vec4(color, 1.0);   // Fully opaque (alpha = 1.0)
```

---

## 8. Weather System Integration

The road shader's wet/rain effects are driven by [`src/simulation/WeatherSystem`](src/simulation/WeatherSystem.h):

**Toggle:** Press **R** key → cycles weather states ([`DownPour.cpp:621-629`](src/DownPour.cpp#L621))

**UBO mapping** ([`DownPour.cpp:230-236`](src/DownPour.cpp#L230)):

| WeatherSystem Method | UBO Field | Shader Variable | Range |
|---------------------|-----------|----------------|-------|
| `getRainIntensity()` | `weatherParams.x` | `rainIntensity` | 0.0 – 1.0 |
| `getWetness()` | `weatherParams.y` | `wetness` | 0.0 – 1.0 |
| `getWind().x` | `weatherParams.z` | (unused by road) | — |
| `getWind().z` | `weatherParams.w` | (unused by road) | — |
| `glfwGetTime()` | `cameraPosition.w` | `elapsedTime` | seconds |

**Shader effects controlled by weather:**

| Effect | Dry (rain=0, wet=0) | Rainy (rain=1, wet=1) |
|--------|--------------------|-----------------------|
| Surface color | Full brightness | ×0.55 (45% darker) |
| Roughness | From texture | → 0.08 (glossy) |
| Splash ripples | None | Animated expanding rings |
| Sun intensity | ×1.0 | ×0.35 (cloud cover) |
| Ambient | (0.22, 0.24, 0.30) | (0.25, 0.26, 0.28) |
| Fog density | 0.0000002 | 0.0000012 (6× denser) |
| Fog color | (0.75, 0.85, 1.0) blue | (0.55, 0.58, 0.62) gray |

---

## 9. Numeric Reference

### Road Geometry

| Parameter | Value | Source |
|-----------|-------|--------|
| Model file | `assets/models/road.glb` | [`DownPour.cpp:757`](src/DownPour.cpp#L757) |
| Scale | (3000, 1000, 1000) | [`DownPour.cpp:776`](src/DownPour.cpp#L776) |
| Position | Origin (0, 0, 0) | [`DownPour.cpp:775`](src/DownPour.cpp#L775) |
| Sidecar JSON | **None** | — |

### Road Markings (World-Space)

| Marking | Position (X) | Width | Color | Pattern |
|---------|-------------|-------|-------|---------|
| Center line | x = 0 | 12cm | Yellow (0.95, 0.8, 0.1) | Dashed: 3m on / 5m off |
| Lane edges | x = ±3.7m | 10cm | White (0.9, 0.9, 0.85) | Dashed: 3m on / 5m off |
| Shoulders | x = ±7.4m | 15cm | White (0.9, 0.9, 0.85) | Solid continuous |

### PBR Parameters

| Parameter | Value | Source |
|-----------|-------|--------|
| F0 (dielectric) | 0.04 | [`world.frag:319`](shaders/world.frag#L319) |
| Sun color | (1.0, 0.98, 0.92) | [`world.frag:332`](shaders/world.frag#L332) |
| Sun intensity | 5.0 | [`DownPour.cpp:214`](src/DownPour.cpp#L214) |
| Marking roughness | 0.35 | [`world.frag:288`](shaders/world.frag#L288) |
| Wet roughness | 0.08 | [`world.frag:297`](shaders/world.frag#L297) |
| Wet darkening | ×0.55 | [`world.frag:296`](shaders/world.frag#L296) |

### Fog

| Parameter | Clear | Rainy | Source |
|-----------|-------|-------|--------|
| Density | 0.0000002 | 0.0000012 | [`world.frag:350`](shaders/world.frag#L350) |
| Color | (0.75, 0.85, 1.0) | (0.55, 0.58, 0.62) | [`world.frag:352`](shaders/world.frag#L352) |

### Splash Ripples

| Parameter | Value | Source |
|-----------|-------|--------|
| Layers | 3 | [`world.frag:172`](shaders/world.frag#L172) |
| Scale range | 1.5 – 2.9 | [`world.frag:173`](shaders/world.frag#L173) |
| Cycle period | 0.6s – 1.0s | [`world.frag:184`](shaders/world.frag#L184) |
| Max ring radius | 0.4 | [`world.frag:190`](shaders/world.frag#L190) |
| Ring width | 0.04 | [`world.frag:191`](shaders/world.frag#L191) |
| Normal bump strength | 0.15 | [`world.frag:303`](shaders/world.frag#L303) |

---

## 10. Key Files Index

| File | What | Key Lines |
|------|------|-----------|
| [`shaders/world.vert`](shaders/world.vert) | Road vertex shader | Transform :25-27, normal matrix :30-31, fog distance :37 |
| [`shaders/world.frag`](shaders/world.frag) | Road fragment shader | Stochastic UV :117-162, road markings :207-243, splash :167-202, PBR :310-337, fog :350-353 |
| [`src/DownPour.cpp`](src/DownPour.cpp) | Application | loadRoadModel :755, createWorldPipeline :721, updateUBO :195, render road :432-460 |
| [`src/DownPour.h`](src/DownPour.h) | Application header | worldPipeline :100, roadAdapter :112, roadModelPtr :116, roadMaterialIds :124 |
| [`src/scene/RoadEntity.h`](src/scene/RoadEntity.h) | Road entity (minimal) | Config struct :17-21 |
| [`src/renderer/ModelAdapter.cpp`](src/renderer/ModelAdapter.cpp) | Model + metadata loader | load() :21-42, loadMetadata :44-300 |
| [`src/renderer/GLTFLoader.cpp`](src/renderer/GLTFLoader.cpp) | GLB parsing | Vertices :91-126, indices :129-145, materials :171-223 |
| [`src/renderer/MaterialManager.cpp`](src/renderer/MaterialManager.cpp) | Material GPU resources | createMaterial :29-147, getDescriptorSet :251-262 |
| [`src/core/PipelineFactory.cpp`](src/core/PipelineFactory.cpp) | Pipeline creation | createPipeline :11-151, vertex input :33-54, depth :95-101 |
| [`src/simulation/WeatherSystem.h`](src/simulation/WeatherSystem.h) | Weather state | Rain intensity, wetness, wind |

---

## 11. How to Modify Road Rendering

### Change road markings layout

Edit [`shaders/world.frag:207-243`](shaders/world.frag#L207). Key parameters:
- Center line position: change `abs(x)` comparison (currently x=0)
- Lane positions: change `3.7` in `abs(abs(x) - 3.7)`
- Shoulder positions: change `7.4` in `abs(abs(x) - 7.4)`
- Dash pattern: change `mod(z, 8.0)` cycle and `3.0` dash length
- Colors: change `yellowColor` and `whiteColor` vec3 values

After editing, recompile: `glslc shaders/world.frag -o shaders/world.frag.spv`

### Change road scale

Edit [`src/DownPour.cpp:776`](src/DownPour.cpp#L776):
```cpp
roadTransform = glm::scale(roadTransform, glm::vec3(WIDTH, HEIGHT, LENGTH));
```

### Add a road sidecar JSON

Create `assets/models/road.glb.json` — ModelAdapter will pick it up automatically. Can define orientation, scale, position offset, just like `bmw_suv.glb.json`.

### Adjust wet effects

Edit [`shaders/world.frag:296-297`](shaders/world.frag#L296):
- Darkening: `0.55` = how dark when fully wet (lower = darker)
- Glossiness: `0.08` = roughness when fully wet (lower = shinier)

### Adjust fog

Edit [`shaders/world.frag:350-352`](shaders/world.frag#L350):
- Density values (remember 1000× scale)
- Fog color for clear and rainy conditions

### Adjust splash ripples

Edit [`shaders/world.frag:167-202`](shaders/world.frag#L167):
- Layer count (`layer < 3`)
- Scale range (`1.5 + layer * 0.7`)
- Cycle speed (`0.6 + h * 0.4`)
- Ring size (`age * 0.4`)
- Bump strength (`splash * 0.15` at line 303)
