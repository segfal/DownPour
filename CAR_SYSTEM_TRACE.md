****# Car Entity System — Code Trace

Complete trace of how the car moves, where the camera sits, how steering/wheels animate, and how it all renders.

---

## 1. Initialization Order

**Entry:** [`src/DownPour.cpp:39-89`](src/DownPour.cpp#L39) — `initVulkan()`

```
initVulkan()
  ├─ ... (Vulkan setup, pipelines, road loading)
  ├─ loadCarModel()       ← src/DownPour.cpp:84
  ├─ initCamera()         ← src/DownPour.cpp:85
  └─ createSyncObjects()
```

---

## 2. Car Model Loading

**Function:** [`src/DownPour.cpp:779-853`](src/DownPour.cpp#L779) — `loadCarModel()`

### 2.1 Load GLB + Sidecar JSON

```
carAdapter->load("assets/models/bmw_suv.glb", ...)     ← src/DownPour.cpp:781
  ├─ Model::loadFromFile()                               ← src/renderer/ModelAdapter.cpp:28
  │   └─ GLTFLoader::load()                              ← src/renderer/GLTFLoader.cpp:19
  │      ├─ Extracts vertices in MESH-LOCAL space        ← GLTFLoader.cpp:91-126
  │      │   (no node transforms applied to vertices)
  │      ├─ Extracts node hierarchy with TRS             ← GLTFLoader.cpp:242-294
  │      └─ Extracts scene root nodes                    ← GLTFLoader.cpp:296-306
  │
  └─ loadMetadata("assets/models/bmw_suv.glb.json")     ← src/renderer/ModelAdapter.cpp:34
     └─ Parses: camera, roles, physics, parts            ← ModelAdapter.cpp:44-300
```

### 2.2 Create CarEntity

[`src/DownPour.cpp:816`](src/DownPour.cpp#L816)
```cpp
carEntity = sceneManager.createEntity<CarEntity>("car", "main", carAdapter);
```

**Constructor:** [`src/scene/CarEntity.cpp:17-32`](src/scene/CarEntity.cpp#L17)
```cpp
CarEntity::CarEntity(const str& name, Scene* scene, ModelAdapter* adapter)
    : Entity(name, scene), adapter(adapter) {
    // Reads physics config from adapter:
    maxSpeed      = 8.0 * 6.0 = 48.0 m/s       ← CarEntity.cpp:24
    accel         = 8.0 m/s²                     ← CarEntity.cpp:25
    brakeForce    = 15.0 m/s²                    ← CarEntity.cpp:26
    maxSteerAngle = 35.0°                        ← CarEntity.cpp:27
    wheelBase     = 2.9 m                        ← CarEntity.cpp:28
    wheelRadius   = 0.38 m                       ← CarEntity.cpp:29
    dragCoeff     = 0.35                         ← CarEntity.cpp:30
    rollingResist = 0.015                        ← CarEntity.cpp:31
}
```

### 2.3 Build Scene Graph from glTF Hierarchy

[`src/DownPour.cpp:819`](src/DownPour.cpp#L819)
```cpp
auto rootHandles = SceneBuilder::buildFromModel(scene, carModelPtr, carMaterialIds);
```

**SceneBuilder:** [`src/scene/SceneBuilder.cpp:8-34`](src/scene/SceneBuilder.cpp#L8)

Creates SceneNodes matching the glTF hierarchy. Each node with a mesh gets `renderData` (indexStart, indexCount, materialId).

**glTF hierarchy inside `bmw_suv.glb`:**
```
[275] Sketchfab_model         S=(1000, 1000, 1000)  R=(-90°X)
  └─[274] FINAL_MODEL_24.fbx  S=(0.01, 0.01, 0.01)  R=(+90°X)
      └─[273] RootNode         (identity)
          ├─ X3:Ani_Wheel_Scale_FL155   (front-left wheel group)
          ├─ X3:Ani_Wheel_Scale_FR151   (front-right wheel group)
          ├─ X3:Ani_Wheel_Scale_BL156   (back-left wheel group)
          ├─ X3:Ani_Wheel_Scale_BR150   (back-right wheel group)
          ├─ X3:Ani_Steer_Wheel_FL_2    (steering wheel)
          ├─ X3:Ani_Hood22              (hood)
          ├─ X3:Ani_Trunk7              (trunk)
          └─ ... (276 nodes total, 143 meshes)
```

**Net scale for mesh vertices:** 1000 × 0.01 = **10×** (rotations cancel out)

Mesh bounding box: `[-1.08, -0.44, -2.35]` to `[1.08, 1.69, 2.36]` (~4.72m long)
After hierarchy: car is ~47 units long in world space.

### 2.4 Map Roles to Scene Nodes

[`src/DownPour.cpp:822-839`](src/DownPour.cpp#L822)

Reads role names from sidecar JSON ([`assets/models/bmw_suv.glb.json:21-32`](assets/models/bmw_suv.glb.json#L21)):

| Role             | glTF Node Name             | Purpose                  |
| ---------------- | -------------------------- | ------------------------ |
| `steering_wheel` | `X3:Ani_Steer_Wheel_FL_2`  | Visual steering rotation |
| `wheel_FL`       | `X3:Ani_Wheel_Scale_FL155` | Front-left wheel group   |
| `wheel_FR`       | `X3:Ani_Wheel_Scale_FR151` | Front-right wheel group  |
| `wheel_BL`       | `X3:Ani_Wheel_Scale_BL156` | Back-left wheel group    |
| `wheel_BR`       | `X3:Ani_Wheel_Scale_BR150` | Back-right wheel group   |
| `hood`           | `X3:Ani_Hood22`            | Hood (future)            |
| `trunk`          | `X3:Ani_Trunk7`            | Trunk (future)           |

Each role is stored via [`Entity::addNode(handle, role)`](src/scene/Entity.cpp#L13) in a `namedNodes` map.

### 2.5 Car Initial Position

[`src/DownPour.cpp:842-846`](src/DownPour.cpp#L842)
```cpp
carRoot->setLocalPosition(glm::vec3(0.0f, 0.0f, 3000.0f));
// Car starts at origin, 3000 units forward on Z
```

---

## 3. Cockpit Camera Setup

### 3.1 Create Camera Node

**Function:** [`src/scene/CarEntity.cpp:49-98`](src/scene/CarEntity.cpp#L49) — `initCockpitCamera()`

```cpp
// Find RootNode (2 levels deep) so camera position is in mesh-space coords
NodeHandle attachParent = scene->findNode("RootNode");     ← CarEntity.cpp:55

// Create camera as child of RootNode
NodeHandle camNode = scene->createNode("cockpit_camera", attachParent);  ← CarEntity.cpp:62
```

**Camera position** (from [`assets/models/bmw_suv.glb.json:13`](assets/models/bmw_suv.glb.json#L13)):
```
cockpitPos = (0.35, 1.2, 0.3)    in mesh-space coordinates
              │     │    └─ Z: +0.3  slightly forward of car center
              │     └──── Y: +1.2  eye height
              └────────── X: +0.35 left of center (LHD driver seat)
```

Applied at [`CarEntity.cpp:77-81`](src/scene/CarEntity.cpp#L77):
```cpp
node->setLocalPosition(cockpitPos);
scene->markSubtreeDirty(camNode);
```

### 3.2 Create Camera Entity

[`src/scene/CarEntity.cpp:84-85`](src/scene/CarEntity.cpp#L84)
```cpp
cockpitCamera = new CameraEntity("cockpit_cam", scene, adapter);
cockpitCamera->addNode(camNode, "camera_root");
```

### 3.3 Camera Config

[`src/scene/CarEntity.cpp:88-97`](src/scene/CarEntity.cpp#L88)
```
FOV        = 75.0°
Near Plane = 0.01
Far Plane  = 10000.0
```

### 3.4 Wire Up in Application

[`src/DownPour.cpp:866-871`](src/DownPour.cpp#L866) — `initCamera()`
```cpp
if (carEntity && carEntity->getCockpitCamera()) {
    cameraEntity = carEntity->getCockpitCamera();   // Use cockpit cam
    cameraEntity->setAspectRatio(aspect);           // 800/600 = 1.333
}
```

### 3.5 Camera Transform Chain

The cockpit camera's world position is computed during `Scene::updateTransforms()`:
```
cockpit_camera.worldTransform =
    carRoot.worldTransform                    (car position + heading rotation)
    × Sketchfab_model.localTransform          (S=1000, R=-90°X)
    × FINAL_MODEL_24.localTransform           (S=0.01, R=+90°X)
    × RootNode.localTransform                 (identity)
    × cockpit_camera.localTransform           (T=(0.35, 1.2, 0.3))
```

### 3.6 View/Projection Matrices

[`src/scene/CameraEntity.cpp:78-88`](src/scene/CameraEntity.cpp#L78)
```cpp
Mat4 CameraEntity::getViewMatrix() const {
    Vec3 position = getWorldPosition();    // worldTransform[3] column
    Vec3 forward = getWorldForward();      // rotation * (0,0,-1)
    Vec3 up = getWorldUp();                // rotation * (0,1,0)
    return glm::lookAt(position, position + forward, up);
}

Mat4 CameraEntity::getProjectionMatrix() const {
    return glm::perspective(glm::radians(75.0f), 1.333f, 0.01f, 10000.0f);
}
```

---

## 4. Main Loop — Per-Frame Update

**Function:** [`src/DownPour.cpp:598-668`](src/DownPour.cpp#L598) — `mainLoop()`

```
Each frame:
  ├─ glfwPollEvents()                                     ← line 600
  ├─ deltaTime = currentTime - lastFrameTime              ← line 602-604
  │
  ├─ ESC: toggle cursor capture                           ← lines 607-618
  ├─ R: cycle weather                                     ← lines 621-629
  │
  ├─ CAR UPDATE (if car exists + cursor captured):        ← lines 632-636
  │   carEntity->update(deltaTime, window)
  │   └─ [See section 5 below]
  │
  ├─ Weather update                                       ← lines 639-650
  ├─ Rain renderer update                                 ← lines 653-655
  │
  ├─ SCENE TRANSFORM PROPAGATION:                         ← lines 658-661
  │   activeScene->updateTransforms()
  │   └─ [See section 7 below]
  │
  ├─ updateUniformBuffer(currentFrame)                    ← line 663
  │   └─ Extracts camera matrices from cockpit camera
  │
  └─ drawFrame()                                          ← line 664
      └─ recordCommandBuffer() → [See section 8]
```

---

## 5. Car Update — What Makes It Move

**Function:** [`src/scene/CarEntity.cpp:100-105`](src/scene/CarEntity.cpp#L100) — `update()`

```cpp
void CarEntity::update(float deltaTime, GLFWwindow* window) {
    updateInput(deltaTime, window);      // Read keys, change speed/steering
    updatePhysics(deltaTime);            // Move car position, rotate heading
    updateState(window);                 // Set state enum
    updateAnimations();                  // Rotate wheels, steering wheel
}
```

### 5.1 Input Handling

**Function:** [`src/scene/CarEntity.cpp:107-143`](src/scene/CarEntity.cpp#L107) — `updateInput()`

| Key | Action | Code | Value |
|-----|--------|------|-------|
| **W** | Throttle | `speed += accel * dt` | +8.0 m/s² |
| **S** | Brake | `speed -= brakeForce * dt` | -15.0 m/s² |
| **A** | Steer left | `steeringAngle += steerSpeed * dt` | +90 °/s |
| **D** | Steer right | `steeringAngle -= steerSpeed * dt` | -90 °/s |
| *none* | Coast | `speed -= (0.147 + 0.00035·v²) * dt` | drag + friction |
| *no A/D* | Return to center | `steeringAngle ±= steerReturn * dt` | 120 °/s |

**Clamping:**
- `speed` clamped to `[0, 48]` m/s ([`CarEntity.cpp:114-115`](src/scene/CarEntity.cpp#L114))
- `steeringAngle` clamped to `[-35°, +35°]` ([`CarEntity.cpp:129-132`](src/scene/CarEntity.cpp#L129))

### 5.2 Physics — Position & Heading

**Function:** [`src/scene/CarEntity.cpp:145-176`](src/scene/CarEntity.cpp#L145) — `updatePhysics()`

**Bicycle steering model** ([`CarEntity.cpp:155-160`](src/scene/CarEntity.cpp#L155)):
```
turnRadius = wheelBase / tan(steeringAngle)     = 2.9 / tan(angle)
angularVel = speed / turnRadius
heading   += angularVel * deltaTime
```

**Position update** ([`CarEntity.cpp:163-165`](src/scene/CarEntity.cpp#L163)):
```
position.x += speed * sin(heading) * deltaTime
position.z += speed * cos(heading) * deltaTime
```

**Heading → rotation** ([`CarEntity.cpp:170`](src/scene/CarEntity.cpp#L170)):
```cpp
node->localRotation = glm::angleAxis(heading, Vec3(0, 1, 0));  // Y-axis rotation
```

**Dirty marking** ([`CarEntity.cpp:172`](src/scene/CarEntity.cpp#L172)):
```cpp
scene->markSubtreeDirty(getRootNode());  // All children need transform recalc
```

**Wheel spin accumulation** ([`CarEntity.cpp:175`](src/scene/CarEntity.cpp#L175)):
```
wheelRotation += (speed / 0.38) * deltaTime     // radians
```
At 10 m/s → 26.3 rad/s → ~4.2 rotations/second

### 5.3 State Machine

**Function:** [`src/scene/CarEntity.cpp:178-192`](src/scene/CarEntity.cpp#L178) — `updateState()`

```
speed < 0.1           → Idle
S pressed + moving    → Braking
A/D pressed + moving  → Turning
otherwise + moving    → Driving
```

---

## 6. Wheel & Steering Animation

**Function:** [`src/scene/CarEntity.cpp:194-215`](src/scene/CarEntity.cpp#L194) — `updateAnimations()`

### 6.1 Wheel Spin

[`CarEntity.cpp:196`](src/scene/CarEntity.cpp#L196):
```cpp
Quat wheelSpin = glm::angleAxis(wheelRotation, Vec3(1, 0, 0));  // X-axis spin
```

### 6.2 Front Wheel Steer + Spin

[`CarEntity.cpp:199-205`](src/scene/CarEntity.cpp#L199):
```cpp
Quat steerQuat = glm::angleAxis(glm::radians(steeringAngle), Vec3(0, 1, 0));  // Y-axis steer
Quat frontWheelRot = steerQuat * wheelSpin;  // Steer first, then spin

animateRotation("wheel_FL", frontWheelRot);  // → X3:Ani_Wheel_Scale_FL155
animateRotation("wheel_FR", frontWheelRot);  // → X3:Ani_Wheel_Scale_FR151
```

### 6.3 Rear Wheels — Spin Only

[`CarEntity.cpp:207-209`](src/scene/CarEntity.cpp#L207):
```cpp
animateRotation("wheel_BL", wheelSpin);      // → X3:Ani_Wheel_Scale_BL156
animateRotation("wheel_BR", wheelSpin);      // → X3:Ani_Wheel_Scale_BR150
```

### 6.4 Steering Wheel Visual

[`CarEntity.cpp:212-214`](src/scene/CarEntity.cpp#L212):
```cpp
float steeringWheelAngle = steeringAngle * (450.0f / 35.0f);  // ±35° input → ±450° visual
Quat steeringRot = glm::angleAxis(glm::radians(steeringWheelAngle), Vec3(0, 0, 1));  // Z-axis

animateRotation("steering_wheel", steeringRot);  // → X3:Ani_Steer_Wheel_FL_2
```

**Ratio:** 1° steering input = 12.86° steering wheel rotation

### 6.5 How animateRotation Works

[`src/scene/Entity.cpp:153-168`](src/scene/Entity.cpp#L153):
```cpp
void Entity::animateRotation(const str& role, const Quat& rotation) {
    NodeHandle handle = getNode(role);       // Lookup from namedNodes map
    SceneNode* node = scene->getNode(handle);
    node->setLocalRotation(rotation);        // Set quaternion
    scene->markSubtreeDirty(handle);         // Invalidate child transforms
}
```

Rotating a parent node (e.g., `wheel_FL`) rotates all its children (tyre mesh + rim mesh) together.

---

## 7. Scene Transform Propagation

**Function:** [`src/scene/Scene.cpp:147-174`](src/scene/Scene.cpp#L147) — `updateTransforms()`

Called once per frame after all updates. BFS from root nodes:

```cpp
for each rootNode:
    queue.push({rootHandle, identity_matrix});

while queue not empty:
    [handle, parentWorld] = queue.pop();
    node = getNode(handle);

    if (node->isStatic && !node->isDirty) continue;   // Skip clean static nodes

    localTransform = T * R * S;                         // ← SceneNode.cpp:11-17
    node->worldTransform = parentWorld * localTransform; // KEY FORMULA

    for each child:
        queue.push({child, node->worldTransform});
```

**SceneNode::getLocalTransform():** [`src/scene/SceneNode.cpp:11-17`](src/scene/SceneNode.cpp#L11)
```cpp
Mat4 transform = glm::translate(Mat4(1.0f), localPosition);
transform = transform * glm::mat4_cast(localRotation);
transform = glm::scale(transform, localScale);
return transform;   // T × R × S
```

---

## 8. Rendering — Per-Node Draw Calls

**Function:** [`src/DownPour.cpp:460-500`](src/DownPour.cpp#L460) — car section of `recordCommandBuffer()`

```cpp
// Bind car vertex/index buffers once
vkCmdBindVertexBuffers(cmd, 0, 1, carVertexBuffers, carOffsets);
vkCmdBindIndexBuffer(cmd, carModelPtr->getIndexBuffer(), 0, VK_INDEX_TYPE_UINT32);

// Get all renderable nodes grouped by model
auto batches = scene->getRenderBatches();              ← Scene.cpp:208

for (const auto& batch : batches) {
    if (batch.model != carModelPtr) continue;          // Only car nodes

    for (const SceneNode* node : batch.nodes) {
        // Each node's worldTransform = full hierarchy chain
        vkCmdPushConstants(..., &node->worldTransform);  // Model matrix

        // Bind this node's material textures
        VkDescriptorSet matDescSet = materialManager->getDescriptorSet(
            node->renderData->materialId, frameIndex);
        vkCmdBindDescriptorSets(..., &matDescSet);

        // Draw this node's index range
        vkCmdDrawIndexed(cmd,
            node->renderData->indexCount,   // How many indices
            1,
            node->renderData->indexStart,   // Offset into index buffer
            0, 0);
    }
}
```

**getRenderBatches():** [`src/scene/Scene.cpp:208-239`](src/scene/Scene.cpp#L208)
- Iterates all active scene nodes
- Groups by `Model*` pointer (car vs road)
- Separates opaque from transparent
- Returns opaque first, then transparent

---

## 9. Camera Matrix Pipeline

**UBO update:** [`src/DownPour.cpp:193-237`](src/DownPour.cpp#L193) — `updateUniformBuffer()`

```
1. view = cameraEntity->getViewMatrix()              ← CameraEntity.cpp:78
   └─ glm::lookAt(worldPos, worldPos + forward, up)

2. proj = cameraEntity->getProjectionMatrix()        ← CameraEntity.cpp:86
   └─ glm::perspective(radians(75°), 1.333, 0.01, 10000)

3. proj[1][1] *= -1                                  ← Flip Y for Vulkan

4. viewProj = proj * view

5. cameraPosition = cockpitCameraNode->worldTransform[3]  ← column 3 = translation
```

**World position extraction:** [`src/scene/CameraEntity.cpp:30-42`](src/scene/CameraEntity.cpp#L30)
```cpp
Vec3 CameraEntity::getWorldPosition() const {
    const SceneNode* node = getScene()->getNode(getRootNode());
    return Vec3(node->worldTransform[3]);  // Translation column
}
```

**Mouse look** still works for cockpit camera via:
[`src/DownPour.cpp:670-693`](src/DownPour.cpp#L670) — `mouseCallback()` → [`CameraEntity::processMouseMovement()`](src/scene/CameraEntity.cpp#L124)

---

## 10. Full Render Order

[`src/DownPour.cpp:412-548`](src/DownPour.cpp#L412) — `recordCommandBuffer()`

| Order | What | Pipeline | Depth |
|-------|------|----------|-------|
| 1 | Road | `worldPipeline` | Writes depth |
| 1.5 | **Car (per-node)** | `worldPipeline` | Writes depth |
| 2 | Terrain (grass) | `terrainPipeline` | Writes depth |
| 3 | Rain particles | `rainPipeline` | Transparent |
| 4 | Skybox | `graphicsPipeline` | depth ≤ 1.0, no write |
| 5 | Screen rain overlay | `screenRainPipeline` | Ignores depth |

---

## 11. Numeric Reference

### Physics

| Parameter | Value | Source |
|-----------|-------|--------|
| Max speed | 48 m/s | [`CarEntity.cpp:24`](src/scene/CarEntity.cpp#L24) |
| Acceleration | 8.0 m/s² | [`bmw_suv.glb.json:57`](assets/models/bmw_suv.glb.json#L57) |
| Brake force | 15.0 m/s² | [`bmw_suv.glb.json:58`](assets/models/bmw_suv.glb.json#L58) |
| Max steer angle | 35° | [`bmw_suv.glb.json:56`](assets/models/bmw_suv.glb.json#L56) |
| Steer speed | 90 °/s | [`CarEntity.h:56`](src/scene/CarEntity.h#L56) |
| Steer return | 120 °/s | [`CarEntity.h:57`](src/scene/CarEntity.h#L57) |
| Wheel base | 2.9 m | [`bmw_suv.glb.json:53`](assets/models/bmw_suv.glb.json#L53) |
| Wheel radius | 0.38 m | [`bmw_suv.glb.json:55`](assets/models/bmw_suv.glb.json#L55) |
| Drag | 0.35 | [`bmw_suv.glb.json:60`](assets/models/bmw_suv.glb.json#L60) |
| Rolling resistance | 0.015 | [`bmw_suv.glb.json:61`](assets/models/bmw_suv.glb.json#L61) |

### Camera

| Parameter | Value | Source |
|-----------|-------|--------|
| Cockpit position | (0.35, 1.2, 0.3) mesh-space | [`bmw_suv.glb.json:13`](assets/models/bmw_suv.glb.json#L13) |
| FOV | 75° | [`bmw_suv.glb.json:15`](assets/models/bmw_suv.glb.json#L15) |
| Near plane | 0.01 | [`bmw_suv.glb.json:16`](assets/models/bmw_suv.glb.json#L16) |
| Far plane | 10000 | [`bmw_suv.glb.json:17`](assets/models/bmw_suv.glb.json#L17) |
| Aspect ratio | 1.333 (800×600) | [`DownPour.cpp:862`](src/DownPour.cpp#L862) |

### Car Initial State

| Parameter | Value | Source |
|-----------|-------|--------|
| Position | (0, 0, 3000) | [`DownPour.cpp:845`](src/DownPour.cpp#L845) |
| Heading | 0 rad (+Z) | [`CarEntity.h:49`](src/scene/CarEntity.h#L49) |
| Speed | 0 m/s | [`CarEntity.h:48`](src/scene/CarEntity.h#L48) |
| Steering | 0° | [`CarEntity.h:50`](src/scene/CarEntity.h#L50) |

### Steering Wheel Ratio

```
visual_angle = input_angle × (450 / 35) = input_angle × 12.86
```
At full lock (±35°) → steering wheel rotates ±450°. Source: [`CarEntity.cpp:212`](src/scene/CarEntity.cpp#L212)

---

## 12. Key Files Index

| File | What | Key Lines |
|------|------|-----------|
| [`src/scene/CarEntity.h`](src/scene/CarEntity.h) | Car class definition | State enum :13, physics fields :43-64, originalRootRot :49-52 |
| [`src/scene/CarEntity.cpp`](src/scene/CarEntity.cpp) | Car implementation | Constructor :18, Camera :50, Input :135, Physics :173, State :227, Animation :243 |
| [`src/scene/CameraEntity.h`](src/scene/CameraEntity.h) | Camera class | Config :36, getters :51-56, input :65-66, setInitialOrientation :69 |
| [`src/scene/CameraEntity.cpp`](src/scene/CameraEntity.cpp) | Camera implementation | WorldPos :30, ViewMatrix :78, Projection :86, MouseLook :124, setInitialOrientation :151 |
| [`src/scene/Entity.h`](src/scene/Entity.h) | Base entity | addNode :23, animateRotation :43 |
| [`src/scene/Entity.cpp`](src/scene/Entity.cpp) | Base entity impl | animateRotation :153 |
| [`src/scene/Scene.cpp`](src/scene/Scene.cpp) | Scene graph | updateTransforms :147, getRenderBatches :208 |
| [`src/scene/SceneNode.h`](src/scene/SceneNode.h) | Node structure | TRS :57-59, RenderData :66-77, worldTransform :62 |
| [`src/scene/SceneNode.cpp`](src/scene/SceneNode.cpp) | Node helpers | getLocalTransform :11 (T×R×S) |
| [`src/scene/SceneBuilder.cpp`](src/scene/SceneBuilder.cpp) | glTF→Scene | buildFromModel :8, createNodeRecursive :36 |
| [`src/renderer/GLTFLoader.cpp`](src/renderer/GLTFLoader.cpp) | GLB parsing | Vertex extraction :91, Hierarchy :242 |
| [`src/renderer/ModelAdapter.cpp`](src/renderer/ModelAdapter.cpp) | Sidecar JSON | Camera :93, Wheels :156, Physics :239, Roles :232 |
| [`src/DownPour.cpp`](src/DownPour.cpp) | Application | loadCarModel :779, initCamera :855, mainLoop :598, render car :460 |
| [`assets/models/bmw_suv.glb.json`](assets/models/bmw_suv.glb.json) | Car config | Camera :12, Roles :21, Physics :53 |

---

## 13. Known Issues — Camera Orientation & Model Geometry

### 13.1 Bug Report (from `Car_Debug.md`)

| # | Issue | Screenshot Observation |
|---|-------|----------------------|
| 1 | Camera facing wrong direction on startup | Looking at passenger seat instead of windshield |
| 2 | Seats don't look like Blender model | Seat geometry/textures appear distorted |
| 3 | Camera changes direction when driving | View swings unpredictably while heading changes |
| 4 | Vertical road artifact | Road surface renders vertically instead of flat |

---

### 13.2 Root Cause — Rotation Overwrite (FIXED)

**The original bug** at [`CarEntity.cpp:170`](src/scene/CarEntity.cpp#L170) (old code):
```cpp
// OLD CODE — DESTRUCTIVE
node->localRotation = glm::angleAxis(heading, Vec3(0.0f, 1.0f, 0.0f));
```

This **overwrote** the Sketchfab_model node's original rotation with a pure Y-axis heading rotation. The glTF hierarchy requires specific rotations to stay correct:

```
Sketchfab_model    rotation: quat(w,x,y,z) from glTF   ← THIS WAS DESTROYED
  FINAL_MODEL_24   rotation: +90°X
    RootNode        identity
```

The Sketchfab_model's rotation (likely -90° around X or Z) exists to cancel FINAL_MODEL_24's +90°X. When overwritten:
- The cancellation breaks → mesh rotated 90° in wrong axis → "vertical road"
- Camera inherits broken hierarchy → wrong direction
- Heading changes compound the error → camera swings

**Fix applied** at [`CarEntity.cpp:185-197`](src/scene/CarEntity.cpp#L185) and [`CarEntity.cpp:218-219`](src/scene/CarEntity.cpp#L218):
```cpp
// Capture original rotation once (lazy, first frame)
if (!hasOriginalRotation) {
    originalRootRotation = node->localRotation;   // ← Sketchfab_model's glTF rotation
    hasOriginalRotation = true;
}

// COMBINE heading with original instead of replacing
Quat headingQuat = glm::angleAxis(heading, Vec3(0.0f, 1.0f, 0.0f));
node->localRotation = headingQuat * originalRootRotation;
```

Members added in [`CarEntity.h:49-52`](src/scene/CarEntity.h#L49):
```cpp
Quat originalRootRotation = Quat(1.0f, 0.0f, 0.0f, 0.0f);
bool hasOriginalRotation = false;
```

---

### 13.3 Root Cause — Camera Initial Rotation Not Set (FIXED)

**The original bug:** `CameraEntity` has `yaw = 180.0f` as a default ([`CameraEntity.h:77`](src/scene/CameraEntity.h#L77)), but `processMouseMovement()` is the only code that converts yaw/pitch into a quaternion and writes it to `node->localRotation`. Until the user moves the mouse, the camera node has **identity rotation** regardless of what `yaw` says.

**Fix applied:** New method [`CameraEntity::setInitialOrientation()`](src/scene/CameraEntity.cpp#L151) sets both:
1. Internal `yaw`/`pitch` fields (so mouse look stays consistent)
2. `node->localRotation` (so the camera is oriented from frame 1)

Called from [`CarEntity.cpp:114`](src/scene/CarEntity.cpp#L114):
```cpp
cockpitCamera->setInitialOrientation(camYaw, camPitch);
```

---

### 13.4 Remaining Issue — Camera Yaw Value Unknown

Even with both fixes above, the camera still points the wrong direction. The issue is: **we don't know which axis is the car's visual forward in mesh space**.

The physics code assumes +Z is forward:
```cpp
position.z += speed * std::cos(heading) * deltaTime;   // ← CarEntity.cpp:212
```

But the Sketchfab_model's rotation may remap axes. The car's visual "hood direction" in mesh space could be +Z, -Z, +X, or -X depending on:
- The original authoring tool (Blender, 3ds Max, etc.)
- The Sketchfab export settings
- The specific quaternion stored in the glTF root node

**Current state:** The camera yaw is data-driven from the sidecar JSON:
```json
// assets/models/bmw_suv.glb.json:15
"rotation": { "euler": [pitch, yaw, roll] }   // degrees
```

Parsed by ModelAdapter at [`ModelAdapter.cpp:100-109`](src/renderer/ModelAdapter.cpp#L100), applied at [`CarEntity.cpp:104-114`](src/scene/CarEntity.cpp#L104).

If the sidecar has `[0, 0, 0]`, the code falls through to the default `yaw = 180.0f` → camera looks along +Z. If the sidecar has a non-zero euler, those values are used instead.

---

## 14. Debug Logging

Debug prints were added to help identify the correct orientation values at runtime.

### 14.1 Cockpit Camera Setup

Printed at the end of [`initCockpitCamera()`](src/scene/CarEntity.cpp#L117):
```
=== Cockpit Camera Debug ===
  Position (mesh space): 0.35, 1.20, 0.30
  Yaw: 0.0 deg, Pitch: 0.0 deg
  World position: X, Y, Z
  World forward:  X, Y, Z
  Tip: Edit 'rotation.euler' [pitch,yaw,roll] in bmw_suv.glb.json to adjust
============================
```

**Key value to look at:** `World forward` — this is the direction the camera is actually looking in world space. Should point in the same direction as the car's hood.

### 14.2 Root Node Rotation Capture

Printed on first frame of [`updatePhysics()`](src/scene/CarEntity.cpp#L188):
```
=== Car Root Rotation Captured ===
  Quat(w,x,y,z): W, X, Y, Z
  Node position: X, Y, Z
  Node scale: X, Y, Z
==================================
```

**Key value to look at:** `Quat(w,x,y,z)` — the actual rotation stored on the Sketchfab_model node. This determines the axis remapping. Common values:

| Quat (w, x, y, z) | Rotation | Meaning |
|--------------------|----------|---------|
| (1, 0, 0, 0) | Identity | No rotation |
| (0.707, -0.707, 0, 0) | -90°X | Y↔Z swap (Blender→glTF common) |
| (-0.707, 0, 0, 0.707) | -90°Z or 270°Z | X↔Y swap |
| (0, 0, 0.707, 0.707) | +90°Z | X↔Y swap (opposite) |

---

## 15. How to Fix the Camera Direction

### Step 1: Get the debug output

Run `make run` and copy the two debug blocks from the console.

### Step 2: Identify the root quaternion

From `Car Root Rotation Captured`, note the `Quat(w,x,y,z)` values. This tells you what Sketchfab_model's rotation actually is.

### Step 3: Determine the visual forward axis

With the root quaternion known, calculate which world-space direction corresponds to +Z in mesh space after applying: `Sketchfab_model_rot × FINAL_MODEL_24_rot × identity`.

Or, simpler: look at the `World forward` from the camera debug. If the camera shows a sideways view, the forward vector is pointing the wrong way. You need a yaw that makes the forward vector align with the car's hood.

### Step 4: Try yaw values in the sidecar JSON

Edit [`assets/models/bmw_suv.glb.json:15`](assets/models/bmw_suv.glb.json#L15):
```json
"rotation": { "euler": [pitch, yaw, roll] }
```

| Yaw value | Camera looks along (mesh space) | Try if... |
|-----------|--------------------------------|-----------|
| `[0, 0, 0]` | -Z | Default, most 3D apps export facing -Z |
| `[0, 180, 0]` | +Z | Physics says +Z is forward |
| `[0, 90, 0]` | -X | Looking sideways? Rotate 90° |
| `[0, -90, 0]` | +X | Looking sideways the other way? |

Run `make run` after each change. No recompile needed — it's JSON.

**Wait — recompile IS needed** because the JSON is loaded at runtime but the binary must be rebuilt if .cpp changed. If only the JSON changed, `make run-only` is enough.

### Step 5: Fine-tune position

Once yaw is correct, adjust the cockpit position in the same JSON:
```json
"position": { "xyz": [X, Y, Z] }
```
- X: lateral (+ = left for LHD, - = right)
- Y: vertical (+ = up, eye height ~1.0-1.3)
- Z: longitudinal (+ or - toward dashboard, depends on model orientation)

---

## 16. Deeper Investigation Areas

If adjusting the sidecar JSON doesn't fully resolve the issues, investigate these areas:

### 16.1 Verify the Actual glTF Quaternion

The quaternion stored in the glTF for Sketchfab_model may not be what we assume. To verify:

**Option A — Check debug output** (easiest):
Run `make run`, read `Quat(w,x,y,z)` from the console.

**Option B — Inspect the GLB directly**:
Use a glTF viewer (e.g., https://gltf-viewer.donmccurdy.com/) or Blender to import `bmw_suv.glb` and check the root node's rotation.

**Option C — Programmatic dump**:
In [`SceneBuilder.cpp`](src/scene/SceneBuilder.cpp), after creating each node, print its TRS. The root node's rotation is the key value.

**What to look for:** If the quat is NOT one of the common 90° rotations (e.g., it has non-zero values in multiple components), the model may have been exported with a non-standard orientation and the entire forward-axis assumption needs revisiting.

### 16.2 Physics Direction vs Visual Direction Mismatch

The physics moves the car along +Z when heading=0:
```cpp
position.z += speed * std::cos(heading) * deltaTime;    // CarEntity.cpp:212
```

But if the visual model faces -Z (or X), the car would appear to drive backward or sideways. Two things to check:

1. **Which direction does the car VISUALLY face on startup?** Open the app with a free-fly camera (temporarily) to see which way the car model is oriented.

2. **Does the car drive forward or backward?** Press W and observe — if the car moves but the scenery goes the wrong way relative to the hood, the physics direction and visual direction are mismatched.

**Fix if mismatched:** Either rotate the movement direction to match the visual:
```cpp
// If visual forward is -Z, flip the physics
position.z -= speed * std::cos(heading) * deltaTime;
```
Or rotate the visual to match the physics (via sidecar JSON model orientation).

### 16.3 Camera Transform Chain Integrity

The cockpit camera world position comes from the scene graph hierarchy:
```
cockpit_camera.worldTransform =
    Sketchfab_model.world    (position + heading * originalRot, scale=1000)
  × FINAL_MODEL_24.local     (rot=+90°X, scale=0.01)
  × RootNode.local            (identity)
  × cockpit_camera.local      (position=(0.35,1.2,0.3), rot=yaw/pitch quat)
```

Each link in this chain can introduce errors. To verify:

1. **Print each node's worldTransform** — Add temporary logging in [`Scene::updateTransforms()`](src/scene/Scene.cpp#L147) to print the worldTransform of `Sketchfab_model`, `FINAL_MODEL_24.fbx`, `RootNode`, and `cockpit_camera` nodes.

2. **Check net rotation** — Extract the 3×3 rotation part from `cockpit_camera.worldTransform`. The forward column should be the camera's look direction. If the rotation part has unexpected values, one of the intermediate nodes has a wrong rotation.

3. **Check for non-identity RootNode** — The trace assumes RootNode is identity, but verify by printing its TRS. Some glTF exporters add hidden rotations.

### 16.4 `processMouseMovement` Overwrite Risk

[`CameraEntity::processMouseMovement()`](src/scene/CameraEntity.cpp#L124) sets `node->localRotation` directly from its internal `yaw`/`pitch`:
```cpp
node->localRotation = yawQuat * pitchQuat;    // CameraEntity.cpp:147
```

This works correctly NOW because `setInitialOrientation()` syncs both:
- `yaw`/`pitch` fields → used by `processMouseMovement`
- `node->localRotation` → used for rendering before mouse input

But be aware: this rotation is in the **camera node's local space** (child of RootNode). The parent hierarchy's rotations (Sketchfab_model, FINAL_MODEL_24) transform it into world space. If those parent rotations change unexpectedly, mouse look will behave strangely.

### 16.5 Seat Textures / Visual Fidelity

Issue #2 ("seats don't look like Blender model") may be separate from the rotation bug. Possible causes:

1. **Missing textures** — Many materials log `using default white` (see startup log). These materials have no baseColor texture in the GLB or the texture failed to load. Check if seat materials are among the `default white` entries.

2. **Normal maps not loaded** — [`GLTFLoader.cpp`](src/renderer/GLTFLoader.cpp) and [`MaterialManager`](src/renderer/MaterialManager.cpp) may not load normal/metallic-roughness maps. PBR without normals looks flat.

3. **Incorrect UV mapping** — If the vertex UV coordinates are wrong, textures map incorrectly. Verify by comparing UV layout in Blender vs what GLTFLoader extracts.

4. **Vertex winding / face culling** — If backface culling is enabled and the model has inverted normals on some meshes, faces will be invisible. Check the pipeline's `VkPipelineRasterizationStateCreateInfo::cullMode`.

### 16.6 Movement Direction After Rotation Fix

With `node->localRotation = headingQuat * originalRootRotation`, the heading rotation is applied in world-space Y axis, THEN the original rotation is applied. This means:

- `headingQuat` rotates around **world Y** (correct for car turning on a flat plane)
- `originalRootRotation` then transforms into the model's coordinate system

This ordering is correct if `originalRootRotation` is a **constant** axis remap. But if the original rotation has a non-trivial component (e.g., a slight tilt), the heading rotation won't be purely horizontal — the car would "tilt" as it turns.

**Verify:** After capturing `originalRootRotation`, check that heading changes only affect yaw (horizontal turning), not pitch or roll.

---

## 17. Sidecar JSON Quick Reference

[`assets/models/bmw_suv.glb.json`](assets/models/bmw_suv.glb.json)

**Camera tuning** — edit these to adjust cockpit view without recompiling:
```json
{
    "camera": {
        "cockpit": {
            "position": { "xyz": [X, Y, Z] },
            "rotation": { "euler": [pitch, yaw, roll] },
            "fov": 75.0,
            "nearPlane": 0.01,
            "farPlane": 10000.0
        }
    }
}
```

| Field | Unit | What it controls |
|-------|------|-----------------|
| `position.xyz[0]` | meters (mesh space) | Lateral: + = left (LHD driver side) |
| `position.xyz[1]` | meters (mesh space) | Vertical: + = up (eye height) |
| `position.xyz[2]` | meters (mesh space) | Longitudinal: + direction depends on model orientation |
| `rotation.euler[0]` | degrees | Pitch: + = look up, - = look down |
| `rotation.euler[1]` | degrees | Yaw: 0 = look -Z, 90 = look -X, 180 = look +Z, -90 = look +X |
| `rotation.euler[2]` | degrees | Roll: not used by cockpit camera (ignored) |
| `fov` | degrees | Vertical field of view |
| `nearPlane` | world units | Near clip distance |
| `farPlane` | world units | Far clip distance |

**Parsing path:** JSON → [`ModelAdapter::loadMetadata()`](src/renderer/ModelAdapter.cpp#L34) → `CameraConfig::CockpitCamera` → read in [`CarEntity::initCockpitCamera()`](src/scene/CarEntity.cpp#L106) → applied via [`CameraEntity::setInitialOrientation()`](src/scene/CameraEntity.cpp#L151)

---

## 18. Files Changed for Bug Fixes

| File | Change | Lines |
|------|--------|-------|
| [`src/scene/CarEntity.h`](src/scene/CarEntity.h) | Added `originalRootRotation`, `hasOriginalRotation` members | :49-52 |
| [`src/scene/CarEntity.cpp`](src/scene/CarEntity.cpp) | Lazy capture of original rotation in `updatePhysics()` | :185-197 |
| [`src/scene/CarEntity.cpp`](src/scene/CarEntity.cpp) | Combine heading with original rotation instead of overwriting | :218-219 |
| [`src/scene/CarEntity.cpp`](src/scene/CarEntity.cpp) | Read sidecar euler rotation in `initCockpitCamera()` | :100-114 |
| [`src/scene/CarEntity.cpp`](src/scene/CarEntity.cpp) | Debug prints for camera and root rotation | :117-126, :188-196 |
| [`src/scene/CameraEntity.h`](src/scene/CameraEntity.h) | Added `setInitialOrientation()` declaration | :68-69 |
| [`src/scene/CameraEntity.cpp`](src/scene/CameraEntity.cpp) | `setInitialOrientation()` implementation — syncs yaw/pitch + node | :151-166 |

---

## 19. Car Shader & Pipeline — Rendering Fix

### 19.1 Problem: Car Rendered with Road Shader

The car was being rendered using `worldPipeline` which uses [`shaders/world.frag`](shaders/world.frag). This shader is designed for the road surface and applies:

| Road Effect | Line | Impact on Car |
|-------------|------|--------------|
| **Stochastic UV sampling** | `world.frag:258` | Randomizes UVs with hash offsets — destroys car texture mapping, causes visual scrambling |
| **Procedural road markings** | `world.frag:285-289` | Yellow center dashes and white lane lines blended into ALL surfaces, including car paint |
| **Wet asphalt darkening** | `world.frag:296` | `baseColor *= 0.55` at full wetness — far too dark for car paint |
| **Splash ripple normals** | `world.frag:300-305` | Animated expanding rings perturb normals on car body panels |

The dedicated car shaders (`car.vert`, `car.frag`) **existed** but were **never wired into a pipeline**.

### 19.2 Fix: Dedicated Car Pipeline

**New files / rewrites:**

| File | What Changed |
|------|-------------|
| [`shaders/car.vert`](shaders/car.vert) | Rewritten to match `world.vert` UBO layout (CameraUBO with sunDirection, cameraPosition, weatherParams). Shares the same push constant (model matrix) and vertex outputs (fragNormal, fragTexCoord, fragWorldPos, fragDistance). |
| [`shaders/car.frag`](shaders/car.frag) | Full rewrite — Cook-Torrance PBR matching world.frag quality, but **no** road markings, **no** stochastic UV, **no** splash ripples. Samples baseColorMap, normalMap, metallicRoughnessMap directly. Handles alpha from baseColor texture. Includes subtle wet surface effect and fog. |
| [`src/DownPour.h`](src/DownPour.h) | Added `VkPipeline carPipeline` member (:100) and `createCarPipeline()` declaration (:183) |
| [`src/DownPour.cpp`](src/DownPour.cpp) | Added `createCarPipeline()` implementation. Car pipeline shares `worldPipelineLayout` (same descriptor sets + push constants). Switched car rendering from `worldPipeline` → `carPipeline`. Added cleanup for `carPipeline`. |

**Pipeline sharing:** `carPipeline` reuses `worldPipelineLayout` because:
- Same camera UBO (set 0, binding 0)
- Same material textures (set 1, bindings 0-2: baseColor, normal, metallicRoughness)
- Same push constant (mat4 model matrix)

Only the shader modules differ.

### 19.3 Car Shader vs World Shader Comparison

| Feature | `world.frag` (road) | `car.frag` (car) |
|---------|---------------------|------------------|
| UV sampling | Stochastic (4× hash-offset blend) | Direct `texture()` call |
| PBR BRDF | Cook-Torrance GGX | Cook-Torrance GGX (identical) |
| Normal mapping | Cotangent-frame TBN | Cotangent-frame TBN (identical) |
| Road markings | Yes (procedural yellow/white lines) | **No** |
| Splash ripples | Yes (animated expanding rings) | **No** |
| Wet darkening | `×0.55` (full wet) | `×0.85` (subtle) |
| Wet smoothing | `roughness → 0.08` | `roughness × 0.5` |
| Fog | Exponential, rain-modulated | Same |
| Tone mapping | Reinhard | Same |
| Alpha output | Always 1.0 (opaque) | From `baseColorMap.a` (supports transparency) |

### 19.4 Render Order (Updated)

```
recordCommandBuffer():
  1.   Road           → worldPipeline   (world.vert + world.frag)
  1.5  Car (per-node) → carPipeline     (car.vert   + car.frag)    ← CHANGED
  2.   Terrain        → terrainPipeline
  3.   Rain           → rainPipeline
  4.   Skybox         → graphicsPipeline
  5.   Screen rain    → screenRainPipeline
```

---

## 20. Car Upside Down — Investigation

### 20.1 Actual Debug Output (captured 2026-02-15)

```
=== Cockpit Camera Debug ===
  Position (mesh space): 0.35, 1.20, 0.30
  Yaw: 0.0 deg, Pitch: 0.0 deg
  World position: 0.0, 0.0, 0.0       ← zero because updateTransforms() hasn't run yet
  World forward:  0.000, 0.000, -1.000 ← looking along -Z (BACKWARD)
============================

=== Car Root Rotation Captured ===
  Quat(w,x,y,z): 0.7071, -0.7071, 0.0000, 0.0000
  Node position: 0.0, 0.0, 3000.0
  Node scale: 1000.0000, 1000.0001, 1000.0001
==================================
```

### 20.2 Decoded Root Rotation

`Quat(0.7071, -0.7071, 0, 0)` decodes as:
- `w = cos(θ/2) = 0.7071` → `θ/2 = 45°` → `θ = 90°`
- `axis = (-0.7071, 0, 0) / 0.7071 = (-1, 0, 0)`
- **Result: -90° rotation around X axis** (equivalent to `glm::angleAxis(radians(-90), Vec3(1,0,0))`)

This is the standard **Sketchfab Z-up → Y-up conversion**. Under -90°X:
- X → X (unchanged)
- Y → -Z
- Z → +Y

Combined with FINAL_MODEL_24's +90°X rotation, they cancel to **identity**. The `headingQuat * originalRootRotation` composition is correct — the car geometry is properly oriented.

### 20.3 Root Cause: Camera Was Facing Backward

The car was NOT upside down. The "upside down" appearance was caused by the **camera looking backward** (-Z) while the car drives forward (+Z).

**Cause:** The sidecar JSON had `"rotation": {"euler": [0, 0, 0]}` which overrode the default yaw=180° to yaw=0°.

| Yaw | Camera forward (local) | After hierarchy (identity net rotation) | Direction |
|-----|----------------------|----------------------------------------|-----------|
| 0° | `(0, 0, -1)` | `(0, 0, -1)` | **-Z = BACKWARD** |
| 180° | `(0, 0, +1)` | `(0, 0, +1)` | **+Z = FORWARD** |

**Fix:** Changed `bmw_suv.glb.json` rotation from `[0, 0, 0]` to `[0, 180, 0]`:
```json
"rotation": { "euler": [0, 180, 0] }
```

### 20.4 Why World Position Was (0,0,0)

The debug print runs during `initCockpitCamera()` which is called from `loadCarModel()` during `initVulkan()`. At this point, `Scene::updateTransforms()` hasn't been called yet (it runs each frame in `mainLoop()`), so all `worldTransform` values are still identity/zero. The forward direction of (0,0,-1) is from the **local** rotation (yaw=0°), not the world-space forward. Once transforms propagate, the actual world forward would incorporate all parent rotations.

---

## 21. Remaining Work / Known Gaps

| Item | Status | What's Needed |
|------|--------|--------------|
| Camera yaw | **FIXED** | Was `[0,0,0]` (looking backward -Z), changed to `[0,180,0]` (looking forward +Z) |
| "Car upside down" | **FIXED** | Was actually camera facing backward, not geometry flipped. Root rotation `Quat(0.7071,-0.7071,0,0)` = -90°X is correct |
| Transparent materials | **Not separated** | Glass/windows use same opaque pipeline — no alpha blending. Need a transparent pass with `enableBlending=true, enableDepthWrite=false` |
| Emissive textures | **Loaded but unused** | MaterialManager loads emissive maps but car.frag doesn't sample a 4th texture. Need to add binding 3 for emissive. |
| Many "default white" materials | **Expected** | ~50% of car materials have no baseColor texture in the GLB. These are solid-colored materials (black plastic, chrome, etc.) that rely on material factors rather than textures. Consider reading glTF `baseColorFactor` from the material and passing it to the shader. |
| Physics direction vs visual | **Unknown** | May need to adjust physics heading if visual forward ≠ +Z after rotation fix |
