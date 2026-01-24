# Mathematical Reference

This document provides a comprehensive reference of all mathematical formulas, equations, and transformations used in the DownPour rain simulator. Each formula includes its mathematical representation, explanation, variable definitions, and references to where it's implemented in the codebase.

## Table of Contents

1. [Camera Mathematics](#1-camera-mathematics)
2. [Car Physics](#2-car-physics)
3. [Rotation and Transformation Matrices](#3-rotation-and-transformation-matrices)
4. [Steering Wheel Animation](#4-steering-wheel-animation)
5. [Wiper Animation](#5-wiper-animation)
6. [Rain Particle Physics](#6-rain-particle-physics)
7. [Alpha Blending (Transparency)](#7-alpha-blending-transparency)
8. [Vulkan Coordinate Transforms](#8-vulkan-coordinate-transforms)

---

## 1. Camera Mathematics

The camera system uses standard computer graphics transformations to convert world coordinates to screen space.

### 1.1 View Matrix (Look-At Transformation)

**Formula:**

$$\mathbf{V} = \text{lookAt}(\mathbf{p}, \mathbf{p} + \mathbf{f}, \mathbf{u})$$

**Where:**
- $\mathbf{V}$ = View matrix (4×4)
- $\mathbf{p}$ = Camera position (world space)
- $\mathbf{f}$ = Forward direction vector (normalized)
- $\mathbf{u}$ = Up vector (normalized)

**Explanation:**

The look-at function creates a view matrix that transforms world coordinates into camera space. The camera looks from position $\mathbf{p}$ toward $\mathbf{p} + \mathbf{f}$, with $\mathbf{u}$ defining the "up" direction.

**Implementation:** [`src/renderer/Camera.cpp:39-41`](src/renderer/Camera.cpp#L39-L41)

```cpp
mat4 Camera::getViewMatrix() {
    return glm::lookAt(position, position + forward, up);
}
```

**Why this approach:**
- Industry-standard camera transformation
- Intuitive for first-person cameras (cockpit view)
- GLM provides optimized implementation

---

### 1.2 Projection Matrix (Perspective Projection)

**Formula:**

$$\mathbf{P} = \text{perspective}(\theta_{\text{fov}}, r_{\text{aspect}}, n, f)$$

**Expanded form:**

$$\mathbf{P} = \begin{bmatrix}
\frac{1}{r_{\text{aspect}} \cdot \tan(\theta_{\text{fov}}/2)} & 0 & 0 & 0 \\
0 & \frac{1}{\tan(\theta_{\text{fov}}/2)} & 0 & 0 \\
0 & 0 & \frac{f}{n - f} & \frac{f \cdot n}{n - f} \\
0 & 0 & -1 & 0
\end{bmatrix}$$

**Where:**
- $\theta_{\text{fov}}$ = Field of view angle (radians)
- $r_{\text{aspect}}$ = Aspect ratio (width / height)
- $n$ = Near clipping plane distance
- $f$ = Far clipping plane distance

**Explanation:**

Creates a perspective projection matrix that transforms camera space coordinates to normalized device coordinates (NDC). Objects farther from the camera appear smaller (perspective foreshortening).

**Implementation:** [`src/renderer/Camera.cpp:43-45`](src/renderer/Camera.cpp#L43-L45)

```cpp
mat4 Camera::getProjectionMatrix() {
    return glm::perspective(glm::radians(fov), aspectRatio, nearPlane, farPlane);
}
```

**Parameter values:**
- $\theta_{\text{fov}}$ = 75° (wider for cockpit immersion)
- $r_{\text{aspect}}$ = 800/600 = 1.333...
- $n$ = 0.1 m
- $f$ = 10,000 m (10 km for long road visibility)

**Why this approach:**
- Standard perspective projection for 3D graphics
- Wide FOV enhances cockpit immersion
- Large far plane ensures entire 6.5 km road is visible

---

### 1.3 Camera Basis Vectors from Euler Angles

**Formulas:**

$$\mathbf{f}_x = \cos(\psi) \cdot \cos(\theta)$$

$$\mathbf{f}_y = \sin(\theta)$$

$$\mathbf{f}_z = \sin(\psi) \cdot \cos(\theta)$$

$$\mathbf{f} = \frac{(\mathbf{f}_x, \mathbf{f}_y, \mathbf{f}_z)}{||(\mathbf{f}_x, \mathbf{f}_y, \mathbf{f}_z)||}$$

$$\mathbf{r} = \frac{\mathbf{f} \times (0, 1, 0)}{||\mathbf{f} \times (0, 1, 0)||}$$

$$\mathbf{u} = \frac{\mathbf{r} \times \mathbf{f}}{||\mathbf{r} \times \mathbf{f}||}$$

**Where:**
- $\psi$ = Yaw angle (horizontal rotation, radians)
- $\theta$ = Pitch angle (vertical rotation, radians)
- $\mathbf{f}$ = Forward direction vector (normalized)
- $\mathbf{r}$ = Right direction vector (normalized)
- $\mathbf{u}$ = Up direction vector (normalized)
- $\times$ = Cross product
- $||\cdot||$ = Vector magnitude (L2 norm)

**Constraints:**
- $\theta \in [-89°, 89°]$ (prevents gimbal lock at zenith/nadir)

**Explanation:**

Converts yaw and pitch angles into an orthonormal basis (forward, right, up) for the camera. The forward vector points where the camera looks, right points to the camera's right, and up points upward relative to the camera.

**Implementation:** [`src/renderer/Camera.cpp:47-55`](src/renderer/Camera.cpp#L47-L55)

```cpp
void Camera::updateCameraVectors() {
    vec3 front;
    front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    front.y = sin(glm::radians(pitch));
    front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    forward = glm::normalize(front);
    right = glm::normalize(glm::cross(forward, vec3(0.0f, 1.0f, 0.0f)));
    up = glm::normalize(glm::cross(right, forward));
}
```

**Why this approach:**
- Euler angles are intuitive for mouse look controls
- Pitch clamping prevents camera flipping
- Gram-Schmidt-like process ensures orthonormal basis
- Industry standard for FPS-style cameras

---

## 2. Car Physics

Simple Euler integration provides responsive arcade-style driving physics.

### 2.1 Velocity Update (Semi-Implicit Euler)

**Formula:**

$$v(t + \Delta t) = v(t) + a \cdot \Delta t$$

**Where:**
- $v(t)$ = Velocity at time $t$ (m/s)
- $a$ = Acceleration (m/s²)
- $\Delta t$ = Time step (seconds)

**Acceleration cases:**

$$a = \begin{cases}
a_{\text{accel}} = 5.0 & \text{if forward input} \\
-a_{\text{brake}} = -8.0 & \text{if backward input} \\
-\text{sgn}(v) \cdot a_{\text{friction}} = -\text{sgn}(v) \cdot 2.0 & \text{if no input}
\end{cases}$$

**Velocity constraints:**

$$v \in [-v_{\text{max}}/2, v_{\text{max}}] = [-7.5, 15.0] \text{ m/s}$$

**Explanation:**

Updates car velocity based on input. Forward accelerates, backward decelerates more quickly (braking), and friction brings the car to rest when no input is applied.

**Implementation:** [`src/DownPour.cpp:1948-1978`](src/DownPour.cpp#L1948-L1978)

**Why this approach:**
- Simple and predictable for arcade-style driving
- Higher deceleration than acceleration feels natural (brakes are powerful)
- Friction prevents perpetual motion
- Asymmetric speed limits allow faster forward than reverse

---

### 2.2 Position Update (Euler Integration)

**Formula:**

$$\mathbf{p}_z(t + \Delta t) = \mathbf{p}_z(t) + v(t + \Delta t) \cdot \Delta t$$

**Where:**
- $\mathbf{p}_z$ = Car position along Z-axis (forward direction)
- $v(t + \Delta t)$ = Updated velocity (from previous equation)

**Explanation:**

Updates car position along the forward axis using the newly computed velocity. This is semi-implicit Euler integration (velocity updated first, then position uses new velocity), which is more stable than explicit Euler.

**Implementation:** [`src/DownPour.cpp:1981`](src/DownPour.cpp#L1981)

```cpp
carPosition.z += carVelocity * deltaTime;
```

**Why this approach:**
- Semi-implicit Euler is more stable than explicit Euler for physics
- Adequate accuracy for arcade driving at typical frame rates (60 Hz)
- Simple to implement and debug

---

### 2.3 Friction Model (Coulomb Friction Approximation)

**Formula:**

$$v(t + \Delta t) = \begin{cases}
v(t) - a_{\text{friction}} \cdot \Delta t & \text{if } v(t) > 0 \text{ and } v(t) > a_{\text{friction}} \cdot \Delta t \\
v(t) + a_{\text{friction}} \cdot \Delta t & \text{if } v(t) < 0 \text{ and } |v(t)| > a_{\text{friction}} \cdot \Delta t \\
0 & \text{if } |v(t)| \le a_{\text{friction}} \cdot \Delta t
\end{cases}$$

**Where:**
- $a_{\text{friction}} = 2.0$ m/s² (friction deceleration magnitude)

**Explanation:**

Applies velocity-dependent friction that brings the car to rest. The third case prevents oscillation around zero by clamping velocity to zero when friction would overshoot.

**Implementation:** [`src/DownPour.cpp:1963-1975`](src/DownPour.cpp#L1963-L1975)

```cpp
if (carVelocity > 0) {
    carVelocity -= friction * deltaTime;
    if (carVelocity < 0) carVelocity = 0;
} else if (carVelocity < 0) {
    carVelocity += friction * deltaTime;
    if (carVelocity > 0) carVelocity = 0;
}
```

**Why this approach:**
- Prevents infinite coasting
- Zero-clamping prevents jitter around v=0
- Simple linear model adequate for arcade feel

---

## 3. Rotation and Transformation Matrices

Transformations convert model coordinates to world space for rendering.

### 3.1 Car Model Transformation Matrix

**Formula:**

$$\mathbf{M}_{\text{car}} = \mathbf{T}(\mathbf{p}_{\text{car}}) \cdot \mathbf{R}_y(\phi_{\text{car}}) \cdot \mathbf{R}_x(270°) \cdot \mathbf{S}(s_{\text{car}})$$

**Expanded:**

$$\mathbf{M}_{\text{car}} = \begin{bmatrix} 1 & 0 & 0 & p_x \\ 0 & 1 & 0 & p_y \\ 0 & 0 & 1 & p_z \\ 0 & 0 & 0 & 1 \end{bmatrix} \cdot \mathbf{R}_y(\phi) \cdot \mathbf{R}_x(270°) \cdot \begin{bmatrix} s & 0 & 0 & 0 \\ 0 & s & 0 & 0 \\ 0 & 0 & s & 0 \\ 0 & 0 & 0 & 1 \end{bmatrix}$$

**Where:**
- $\mathbf{T}$ = Translation matrix
- $\mathbf{R}_y(\phi)$ = Rotation around Y-axis (steering)
- $\mathbf{R}_x(270°)$ = Rotation around X-axis (model orientation correction)
- $\mathbf{S}(s)$ = Uniform scale matrix
- $\mathbf{p}_{\text{car}}$ = Car world position
- $\phi_{\text{car}}$ = Car rotation angle (currently unused for steering)
- $s_{\text{car}}$ = Scale factor (calculated from model bounds)

**Rotation matrices:**

$$\mathbf{R}_y(\phi) = \begin{bmatrix}
\cos(\phi) & 0 & \sin(\phi) & 0 \\
0 & 1 & 0 & 0 \\
-\sin(\phi) & 0 & \cos(\phi) & 0 \\
0 & 0 & 0 & 1
\end{bmatrix}$$

$$\mathbf{R}_x(270°) = \begin{bmatrix}
1 & 0 & 0 & 0 \\
0 & 0 & 1 & 0 \\
0 & -1 & 0 & 0 \\
0 & 0 & 0 & 1
\end{bmatrix}$$

**Explanation:**

Transforms the car model from model space to world space. The 270° X-rotation corrects the model's orientation (it was modeled pointing up the Y-axis). Transformations are applied right-to-left: scale, rotate to fix orientation, rotate for steering, translate to world position.

**Implementation:** [`src/DownPour.cpp:1984-1991`](src/DownPour.cpp#L1984-L1991)

```cpp
glm::mat4 modelMatrix = glm::mat4(1.0f);
modelMatrix = glm::translate(modelMatrix, carPosition);
modelMatrix = glm::rotate(modelMatrix, glm::radians(carRotation), glm::vec3(0.0f, 1.0f, 0.0f));
modelMatrix = glm::rotate(modelMatrix, glm::radians(270.0f), glm::vec3(1.0f, 0.0f, 0.0f));
modelMatrix = glm::scale(modelMatrix, glm::vec3(carScaleFactor, carScaleFactor, carScaleFactor));
```

**Why this approach:**
- Standard hierarchical transformation model
- Right-to-left composition matches mathematical convention
- Model orientation fix is required only once (baked into pipeline)
- Uniform scale maintains aspect ratio

---

### 3.2 Cockpit Camera Position Transformation

**Formula:**

$$\mathbf{R}_{\text{cockpit}} = \mathbf{R}_y(\phi_{\text{car}}) \cdot \mathbf{R}_x(90°)$$

$$\mathbf{o}_{\text{rotated}} = \mathbf{R}_{\text{cockpit}} \cdot \begin{bmatrix} o_x \\ o_y \\ o_z \\ 0 \end{bmatrix}$$

$$\mathbf{p}_{\text{camera}} = \mathbf{p}_{\text{car}} + \mathbf{o}_{\text{rotated}} + \mathbf{a}$$

**Where:**
- $\mathbf{R}_{\text{cockpit}}$ = Combined rotation matrix
- $\mathbf{o} = (0.0, -0.21, -0.18)$ = Cockpit offset in model space
- $\mathbf{o}_{\text{rotated}}$ = Offset transformed to world space
- $\mathbf{a} = (0.0, 0.4, -0.15)$ = Additional adjustment offset
- $\mathbf{p}_{\text{camera}}$ = Final camera position

**Explanation:**

Transforms the cockpit offset from model space to world space, accounting for the car's rotation and the 90° model orientation fix (note: 90° here, not 270° like model matrix, because we're transforming a vector, not the entire model). The camera is positioned inside the car at the driver's eye level.

**Implementation:** [`src/DownPour.cpp:2023-2057`](src/DownPour.cpp#L2023-L2057)

```cpp
glm::mat4 rotationMatrix = glm::mat4(1.0f);
rotationMatrix = glm::rotate(rotationMatrix, glm::radians(carRotation), glm::vec3(0.0f, 1.0f, 0.0f));
rotationMatrix = glm::rotate(rotationMatrix, glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));

glm::vec3 rotatedOffset = glm::vec3(rotationMatrix * glm::vec4(cockpitOffset, 0.0f));
glm::vec3 cockpitPosition = carPosition + rotatedOffset + glm::vec3(0.0f, 0.4f, -0.15f);
camera.setPosition(cockpitPosition);
```

**Why this approach:**
- Keeps camera locked to car (no lag or smoothing yet)
- Homogeneous coordinates (w=0) indicate direction vector, not position
- Additional offset fine-tunes eye position empirically
- 90° vs 270° difference because we're rotating offset direction, not the model

---

## 4. Steering Wheel Animation

The steering wheel rotation provides visual feedback for steering input.

### 4.1 Steering Wheel Rotation Update

**Formula:**

$$\theta(t + \Delta t) = \text{clamp}\left(\theta(t) + \Delta\theta, -\theta_{\text{max}}, \theta_{\text{max}}\right)$$

**Where:**

$$\Delta\theta = \begin{cases}
+\omega_{\text{steer}} \cdot \Delta t & \text{if turning left} \\
-\omega_{\text{steer}} \cdot \Delta t & \text{if turning right} \\
-\text{sgn}(\theta) \cdot \omega_{\text{return}} \cdot \Delta t & \text{if no input (return to center)}
\end{cases}$$

**Parameters:**
- $\theta$ = Current steering wheel angle (degrees)
- $\theta_{\text{max}} = 450°$ = Maximum rotation (1.25 full turns)
- $\omega_{\text{steer}} = 180°$/s = Steering rotation speed
- $\omega_{\text{return}} = 360°$/s = Return-to-center speed
- $\Delta t$ = Time step (seconds)

**Explanation:**

Updates steering wheel rotation based on input. The wheel can rotate up to 450° (±1.25 turns). When input is released, it returns to center position at double the steering speed.

**Implementation:** [`src/DownPour.cpp:1994-2020`](src/DownPour.cpp#L1994-L2020)

```cpp
const float maxSteeringAngle = 450.0f;
const float steeringSpeed = 180.0f;
const float returnSpeed = 360.0f;

if (turningLeft && !turningRight) {
    steeringWheelRotation += steeringSpeed * deltaTime;
    steeringWheelRotation = glm::min(steeringWheelRotation, maxSteeringAngle);
} else if (turningRight && !turningLeft) {
    steeringWheelRotation -= steeringSpeed * deltaTime;
    steeringWheelRotation = glm::max(steeringWheelRotation, -maxSteeringAngle);
} else {
    // Return to center logic
}
```

**Why this approach:**
- Visual feedback for player input
- Realistic steering wheel range (1.25 turns is typical for sports cars)
- Fast return-to-center feels responsive
- Clamping prevents unrealistic over-rotation

---

### 4.2 Return to Center with Zero Clamping

**Formula:**

$$\theta(t + \Delta t) = \begin{cases}
\max(0, \theta(t) - \omega_{\text{return}} \cdot \Delta t) & \text{if } \theta(t) > 0 \\
\min(0, \theta(t) + \omega_{\text{return}} \cdot \Delta t) & \text{if } \theta(t) < 0 \\
0 & \text{if } |\theta(t)| \le \omega_{\text{return}} \cdot \Delta t
\end{cases}$$

**Explanation:**

Returns steering wheel to center (0°) when no input is detected. Zero clamping prevents oscillation around the center position.

**Implementation:** [`src/DownPour.cpp:2012-2018`](src/DownPour.cpp#L2012-L2018)

```cpp
if (steeringWheelRotation > 0.0f) {
    steeringWheelRotation -= returnSpeed * deltaTime;
    if (steeringWheelRotation < 0.0f) steeringWheelRotation = 0.0f;
} else if (steeringWheelRotation < 0.0f) {
    steeringWheelRotation += returnSpeed * deltaTime;
    if (steeringWheelRotation > 0.0f) steeringWheelRotation = 0.0f;
}
```

**Why this approach:**
- Natural feel for steering wheel (returns to center when released)
- Zero clamping prevents visual jitter
- Similar to friction model for car velocity

---

## 5. Wiper Animation

Windshield wipers oscillate between angular bounds when activated.

### 5.1 Wiper Angle Oscillation

**Formula:**

$$\theta_{\text{wiper}}(t + \Delta t) = \theta_{\text{wiper}}(t) + d \cdot \omega_{\text{wiper}} \cdot \Delta t$$

**With boundary conditions:**

$$\text{if } \theta \ge \theta_{\text{max}}: \quad \theta = \theta_{\text{max}}, \quad d \leftarrow -d$$

$$\text{if } \theta \le \theta_{\text{min}}: \quad \theta = \theta_{\text{min}}, \quad d \leftarrow -d$$

**Where:**
- $\theta_{\text{wiper}}$ = Current wiper angle
- $\theta_{\text{min}} = -45°$ = Leftmost position
- $\theta_{\text{max}} = +45°$ = Rightmost position
- $d \in \{-1, +1\}$ = Direction flag
- $\omega_{\text{wiper}} = 90°$/s = Wiper angular velocity

**Sweep period:**

$$T_{\text{sweep}} = \frac{2(\theta_{\text{max}} - \theta_{\text{min}})}{\omega_{\text{wiper}}} = \frac{2 \cdot 90°}{90°/\text{s}} = 2 \text{ s per half-sweep, } 4 \text{ s full cycle}$$

**Explanation:**

Wiper blade oscillates between -45° and +45°, reversing direction at boundaries. The wiper completes a full cycle (left to right and back) in 4 seconds, typical for car wipers.

**Implementation:** [`src/simulation/WindshieldSurface.cpp:53-70`](src/simulation/WindshieldSurface.cpp#L53-L70)

```cpp
float movement = wiperSpeed * deltaTime;
if (wiperDirection) {
    wiperAngle += movement;
    if (wiperAngle >= 45.0f) {
        wiperAngle = 45.0f;
        wiperDirection = false;
    }
} else {
    wiperAngle -= movement;
    if (wiperAngle <= -45.0f) {
        wiperAngle = -45.0f;
        wiperDirection = true;
    }
}
```

**Why this approach:**
- Simple harmonic-like motion (though not sinusoidal)
- Realistic wiper speed (60-80 cycles/min typical for cars)
- Direction reversal at boundaries creates oscillation
- Easy to extend with variable speeds (intermittent, fast, slow)

---

## 6. Rain Particle Physics

Rain particles follow simplified ballistic motion under gravity.

### 6.1 Raindrop Spawn Position (Uniform Random Distribution)

**Formula:**

$$\mathbf{p}_{\text{spawn}} = \begin{pmatrix} U(-r, +r) \\ h \\ U(-r, +r) \end{pmatrix}$$

**Where:**
- $U(a, b)$ = Uniform random distribution in $[a, b]$
- $r = 20$ m = Spawn radius
- $h = 30$ m = Spawn height

**Explanation:**

Raindrops spawn in a 40 m × 40 m horizontal area at 30 m altitude. Uniform random distribution creates even rain coverage.

**Implementation:** [`src/simulation/WeatherSystem.cpp:38-55`](src/simulation/WeatherSystem.cpp#L38-L55)

```cpp
drop.position = glm::vec3(
    distPos(gen),       // -20 to +20m
    SPAWN_HEIGHT,       // 30m
    distPos(gen)        // -20 to +20m
);
```

**Why this approach:**
- Simple and fast (uniform distribution)
- Spawn volume follows camera (future enhancement)
- 30 m height gives ~2.5s fall time before ground impact
- Future: Use blue noise or Poisson disk for better distribution

---

### 6.2 Raindrop Velocity and Position Update

**Formulas:**

$$\mathbf{v} = \begin{pmatrix} 0 \\ -g \\ 0 \end{pmatrix} = \begin{pmatrix} 0 \\ -9.8 \\ 0 \end{pmatrix} \text{ m/s}^2$$

$$\mathbf{p}(t + \Delta t) = \mathbf{p}(t) + \mathbf{v} \cdot \Delta t$$

**Where:**
- $g = 9.8$ m/s² = Gravitational acceleration (Earth standard)
- $\mathbf{v}$ = Constant velocity (no air resistance)
- $\mathbf{p}$ = Particle position

**Deactivation conditions:**

$$\text{deactivate if } y < 0 \text{ or } t_{\text{life}} > 10 \text{ s}$$

**Explanation:**

Simple ballistic motion with constant downward velocity. No air resistance or terminal velocity (simplification for performance). Particles deactivate when hitting ground or exceeding lifetime.

**Implementation:** [`src/simulation/WeatherSystem.cpp:57-70`](src/simulation/WeatherSystem.cpp#L57-L70)

```cpp
drop.velocity = glm::vec3(0.0f, -9.8f, 0.0f);
drop.position += drop.velocity * deltaTime;

if (drop.position.y < 0.0f || drop.lifetime > 10.0f) {
    drop.active = false;
}
```

**Why this approach:**
- Constant velocity is much faster than terminal velocity calculation
- Rain falls fast enough that simplified physics looks convincing
- 10 s lifetime prevents particles getting stuck
- Future: Add wind force, terminal velocity for realism

---

## 7. Alpha Blending (Transparency)

Transparent surfaces (glass windows) use alpha blending to composite with scene.

### 7.1 Alpha Blending Equation

**Formula:**

$$\mathbf{C}_{\text{out}} = \mathbf{C}_{\text{src}} \cdot \alpha_{\text{src}} + \mathbf{C}_{\text{dst}} \cdot (1 - \alpha_{\text{src}})$$

$$\alpha_{\text{out}} = \alpha_{\text{src}} \cdot 1 + \alpha_{\text{dst}} \cdot 0 = \alpha_{\text{src}}$$

**Where:**
- $\mathbf{C}_{\text{src}}$ = Source color (transparent fragment)
- $\mathbf{C}_{\text{dst}}$ = Destination color (framebuffer)
- $\alpha_{\text{src}} \in [0, 1]$ = Source alpha (0 = fully transparent, 1 = opaque)
- $\mathbf{C}_{\text{out}}$ = Final blended color

**Vulkan blend factors:**
- Source color: `VK_BLEND_FACTOR_SRC_ALPHA`
- Destination color: `VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA`
- Blend operation: `VK_BLEND_OP_ADD`

**Explanation:**

Standard alpha blending (Porter-Duff "over" operator). Source fragment is blended over destination based on its alpha value. For $\alpha = 0.5$, result is 50% source + 50% destination.

**Implementation:** [`src/DownPour.cpp:1848-1853`](src/DownPour.cpp#L1848-L1853)

```cpp
colorBlendAttachment.blendEnable = VK_TRUE;
colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
```

**Rendering order:**
1. Opaque objects (depth write enabled)
2. Transparent objects (depth write disabled, depth test enabled)

**Why this approach:**
- Industry standard for transparency
- Physically plausible for glass materials
- Requires back-to-front sorting for correct order-independent transparency (OIT)
- Depth write disabled prevents transparent surfaces from occluding objects behind them

---

### 7.2 Depth Testing for Transparency

**Depth test configuration:**

$$\text{if } z_{\text{fragment}} < z_{\text{buffer}}: \text{pass, but DON'T write } z_{\text{fragment}}$$

**Vulkan configuration:**
- `depthTestEnable = VK_TRUE`
- `depthWriteEnable = VK_FALSE`
- `depthCompareOp = VK_COMPARE_OP_LESS`

**Explanation:**

Transparent objects read from depth buffer but don't write to it. This prevents transparent surfaces from incorrectly occluding other transparent or opaque objects behind them.

**Why this approach:**
- Prevents transparency sorting artifacts for single-layer transparency
- Works well for car windows (only one glass layer visible at a time from any angle)
- Simpler than order-independent transparency techniques

---

## 8. Vulkan Coordinate Transforms

Vulkan and OpenGL use different NDC coordinate systems requiring correction.

### 8.1 Vulkan Y-Axis Flip Correction

**Formula:**

$$\mathbf{P}_{\text{Vulkan}} = \mathbf{F} \cdot \mathbf{P}_{\text{GLM}}$$

**Where:**

$$\mathbf{F} = \begin{bmatrix}
1 & 0 & 0 & 0 \\
0 & -1 & 0 & 0 \\
0 & 0 & 1 & 0 \\
0 & 0 & 0 & 1
\end{bmatrix}$$

**Or equivalently:**

$$\mathbf{P}_{\text{Vulkan}}[1][1] = -\mathbf{P}_{\text{GLM}}[1][1]$$

**Coordinate system differences:**

| Aspect | OpenGL / GLM | Vulkan |
|--------|--------------|--------|
| Y-axis | Up (+1 top) | Down (+1 bottom) |
| Z-axis | [-1, +1] | [0, +1] |
| Origin | Bottom-left | Top-left |

**Explanation:**

GLM produces OpenGL-style projection matrices with Y-up. Vulkan expects Y-down. Negating the [1][1] element (Y-scale) flips the Y-axis to match Vulkan's coordinate system.

**Implementation:** [`src/DownPour.cpp:247`](src/DownPour.cpp#L247)

```cpp
ubo.proj = camera.getProjectionMatrix();
ubo.proj[1][1] *= -1;  // GLM for OpenGL, flip Y for Vulkan
```

**Why this approach:**
- Minimal modification to use GLM with Vulkan
- Alternative: use GLM_FORCE_DEPTH_ZERO_TO_ONE and clip space matrix
- Only affects projection matrix, not view matrix
- Common pattern in Vulkan applications using GLM

---

## Mathematical Notation Conventions

### Symbols

- **Vectors:** Bold lowercase ($\mathbf{v}$, $\mathbf{p}$)
- **Matrices:** Bold uppercase ($\mathbf{M}$, $\mathbf{R}$)
- **Scalars:** Regular font ($t$, $\theta$, $\alpha$)
- **Functions:** Roman font ($\sin$, $\cos$, $\text{clamp}$)

### Operations

- $\cdot$ = Scalar multiplication or dot product (context-dependent)
- $\times$ = Cross product (vectors)
- $||\mathbf{v}||$ = Vector magnitude (L2 norm)
- $\text{sgn}(x)$ = Sign function: +1 if $x > 0$, -1 if $x < 0$, 0 if $x = 0$
- $\text{clamp}(x, a, b)$ = $\max(a, \min(b, x))$

### Coordinate Systems

- **Model Space:** Local to 3D model (before transformations)
- **World Space:** Global coordinate system (after model matrix)
- **View Space (Camera Space):** Relative to camera (after view matrix)
- **Clip Space:** After projection matrix, before perspective divide
- **NDC (Normalized Device Coordinates):** After perspective divide, [-1,+1] or [0,1]
- **Screen Space:** Final pixel coordinates

---

## Units

All quantities use SI units unless otherwise specified:

- **Distance:** meters (m)
- **Time:** seconds (s)
- **Velocity:** meters per second (m/s)
- **Acceleration:** meters per second squared (m/s²)
- **Angles:** degrees (°) in code, radians in formulas
- **Mass:** No mass modeling (simplified physics)

---

## Performance Considerations

### Numerical Stability

- **Euler Integration:** Stable for typical frame rates (60 Hz, Δt ≈ 0.016 s)
- **Zero Clamping:** Prevents jitter in friction and steering return-to-center
- **Pitch Clamping:** Prevents gimbal lock at camera zenith/nadir

### Optimization Opportunities

1. **Rain Particles:**
   - Current: CPU-side update, could move to compute shader
   - Benefit: Update 5000 particles in parallel on GPU
   
2. **Camera Basis:**
   - Current: Recalculated every mouse movement
   - Optimization: Cache if yaw/pitch unchanged
   
3. **Matrix Operations:**
   - GLM operations are SIMD-optimized on most platforms
   - Further optimization: Quaternions for rotations (avoids gimbal lock)

---

## Future Enhancements

### Physics Improvements

1. **Terminal Velocity:** $v_{\text{terminal}} = \sqrt{\frac{2mg}{\rho C_D A}}$ for realistic rain
2. **Wind Force:** $\mathbf{F}_{\text{wind}} = -k\mathbf{v}_{\text{wind}}$ for diagonal rain
3. **Car Steering:** Ackermann steering geometry for realistic turning
4. **Suspension:** Spring-damper model for camera shake

### Rendering Improvements

1. **Order-Independent Transparency (OIT):** Weighted blended OIT or depth peeling
2. **Motion Blur:** Velocity buffer for rain streaks
3. **HDR Rendering:** Exposure for realistic lighting

---

## References

### Mathematics
- **Linear Algebra:** *Foundations of Game Engine Development, Vol. 1: Mathematics* by Eric Lengyel
- **3D Rotations:** *Visualizing Quaternions* by Andrew Hanson
- **Projection Matrices:** *Real-Time Rendering, 4th Edition* by Akenine-Möller et al.

### Physics
- **Game Physics:** *Game Physics Engine Development* by Ian Millington
- **Numerical Integration:** *Physically Based Modeling: Principles and Practice* (SIGGRAPH course notes)

### Graphics
- **Vulkan:** [Vulkan Specification 1.3](https://www.khronos.org/registry/vulkan/)
- **GLM:** [GLM Documentation](https://github.com/g-truc/glm)
- **Alpha Blending:** [Porter-Duff Compositing](https://keithp.com/~keithp/porterduff/p253-porter.pdf)

---

*Last Updated: January 2026*
