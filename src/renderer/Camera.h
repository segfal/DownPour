// SPDX-License-Identifier: MIT
#pragma once
#include "core/Types.h"

#include <GLFW/glfw3.h>
// logger
#include "logger/Logger.h"

using namespace DownPour::Types;


typedef Vec3  vec3;
typedef Mat3  mat3;
typedef Vec4  vec4;
typedef Mat4  mat4;
typedef Quat  quat;
typedef float f32;


enum class CameraMode {
    Cockpit,     
    Chase,       
    ThirdPerson  
};

class Camera {
public:
    Camera(vec3 pos, f32 aspect);
    Camera(vec3 pos, f32 aspect, f32 yaw, f32 pitch);
    Camera() : Camera(vec3(0.0f, 0.0f, 0.0f), 4.0f / 3.0f) {}

    // Matrix getters
    mat4 getProjectionMatrix();
    mat4 getViewMatrix();

    // Position getter
    vec3 getPosition() const { return position; }

    void processInput(GLFWwindow* window, float deltaTime);   // for WASD movement
    void processMouseMovement(float xoffset, float yoffset);  // for looking around


    void       setMode(CameraMode mode);
    CameraMode getMode() const { return currentMode; }
    void       cycleMode();  


    void setCameraTarget(const vec3& target, const quat& targetRotation);
    void setCockpitOffset(const vec3& offset) { cockpitOffset = offset; }
    void updateCameraMode(float deltaTime);  


    void setYaw(f32 newYaw);
    void setPitch(f32 newPitch);
    void setPosition(vec3 pos);
    void setFOV(f32 newFOV);
    void setFarPlane(f32 newFarPlane);


    void setChaseDistance(f32 distance) { chaseDistance = distance; }
    void setChaseHeight(f32 height) { chaseHeight = height; }
    void setThirdPersonDistance(f32 distance) { thirdPersonDistance = distance; }

private:

    vec3 position;
    vec3 forward, right, up;
    f32  pitch, yaw;
    f32  fov, aspectRatio, nearPlane, farPlane;

    
    CameraMode currentMode = CameraMode::Cockpit;

    
    vec3 targetPosition = vec3(0.0f);
    quat targetRotation = quat(1.0f, 0.0f, 0.0f, 0.0f);

    
    vec3 cockpitOffset = vec3(0.0f, 0.5f, 0.5f);  

    f32  chaseDistance        = 5.0f;        
    f32  chaseHeight          = 1.5f;        
    f32  chaseStiffness       = 5.0f;        
    vec3 chaseCurrentVelocity = vec3(0.0f);  

    f32 thirdPersonDistance = 8.0f;  
    f32 thirdPersonHeight   = 3.0f;  
    f32 thirdPersonAngle    = 0.0f;  

    void updateCameraVectors();               
    void updateCockpitCamera();               
    void updateChaseCamera(float deltaTime);  
    void updateThirdPersonCamera();           
};
