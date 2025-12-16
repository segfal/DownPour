use serde::{Deserialize, Serialize};
use std::fs;
use std::path::Path;

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct SceneConfig {
    #[serde(default)]
    pub model: ModelConfig,
    #[serde(default)]
    pub camera: CameraConfig,
    #[serde(default)]
    pub objects: Vec<SceneObject>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ModelConfig {
    #[serde(rename = "targetLength", default = "default_target_length")]
    pub target_length: f32,
    #[serde(default)]
    pub orientation: Orientation,
    #[serde(default)]
    pub scale: Scale,
    #[serde(rename = "positionOffset", default)]
    pub position_offset: [f32; 3],
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Orientation {
    #[serde(default)]
    pub euler: [f32; 3],
    #[serde(default = "default_unit")]
    pub unit: String,
    #[serde(default = "default_order")]
    pub order: String,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Scale {
    #[serde(default = "default_scale")]
    pub uniform: f32,
    #[serde(default = "default_scale_xyz")]
    pub xyz: [f32; 3],
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct CameraConfig {
    #[serde(default)]
    pub cockpit: CockpitCamera,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct CockpitCamera {
    #[serde(default)]
    pub position: Position,
    #[serde(default)]
    pub rotation: Rotation,
    #[serde(default = "default_fov")]
    pub fov: f32,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Position {
    #[serde(default)]
    pub xyz: [f32; 3],
    #[serde(default)]
    pub description: String,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Rotation {
    #[serde(default)]
    pub quaternion: [f32; 4],
    #[serde(default)]
    pub euler: [f32; 3],
    #[serde(rename = "eulerUnit", default = "default_unit")]
    pub euler_unit: String,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct SceneObject {
    pub name: String,
    pub transform: Transform,
    #[serde(default = "default_enabled")]
    pub enabled: bool,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Transform {
    #[serde(default)]
    pub position: [f32; 3],
    #[serde(rename = "rotationEuler", default)]
    pub rotation_euler: [f32; 3],  // yaw, pitch, roll in degrees
    #[serde(default = "default_scale_xyz")]
    pub scale: [f32; 3],
}

// Default value functions
fn default_target_length() -> f32 {
    4.7
}

fn default_unit() -> String {
    "degrees".to_string()
}

fn default_order() -> String {
    "XYZ".to_string()
}

fn default_scale() -> f32 {
    1.0
}

fn default_scale_xyz() -> [f32; 3] {
    [1.0, 1.0, 1.0]
}

fn default_fov() -> f32 {
    75.0
}

fn default_enabled() -> bool {
    true
}

impl Default for ModelConfig {
    fn default() -> Self {
        Self {
            target_length: default_target_length(),
            orientation: Orientation::default(),
            scale: Scale::default(),
            position_offset: [0.0, 0.0, 0.0],
        }
    }
}

impl Default for Orientation {
    fn default() -> Self {
        Self {
            euler: [0.0, 0.0, 0.0],
            unit: default_unit(),
            order: default_order(),
        }
    }
}

impl Default for Scale {
    fn default() -> Self {
        Self {
            uniform: default_scale(),
            xyz: default_scale_xyz(),
        }
    }
}

impl Default for CameraConfig {
    fn default() -> Self {
        Self {
            cockpit: CockpitCamera::default(),
        }
    }
}

impl Default for CockpitCamera {
    fn default() -> Self {
        Self {
            position: Position::default(),
            rotation: Rotation::default(),
            fov: default_fov(),
        }
    }
}

impl Default for Position {
    fn default() -> Self {
        Self {
            xyz: [0.0, 0.0, 0.0],
            description: String::new(),
        }
    }
}

impl Default for Rotation {
    fn default() -> Self {
        Self {
            quaternion: [0.0, 0.0, 0.0, 1.0],
            euler: [0.0, 0.0, 0.0],
            euler_unit: default_unit(),
        }
    }
}

impl SceneConfig {
    /// Load scene configuration from JSON file
    pub fn load<P: AsRef<Path>>(path: P) -> Result<Self, Box<dyn std::error::Error>> {
        let json_str = fs::read_to_string(path)?;
        let config: SceneConfig = serde_json::from_str(&json_str)?;
        Ok(config)
    }

    /// Save scene configuration to JSON file
    pub fn save<P: AsRef<Path>>(&self, path: P) -> Result<(), Box<dyn std::error::Error>> {
        let json_str = serde_json::to_string_pretty(self)?;
        fs::write(path, json_str)?;
        Ok(())
    }

    /// Create example scene with model and camera configuration
    pub fn example() -> Self {
        Self {
            model: ModelConfig {
                target_length: 4.7,
                orientation: Orientation {
                    euler: [0.0, 180.0, 0.0],
                    unit: "degrees".to_string(),
                    order: "XYZ".to_string(),
                },
                scale: Scale {
                    uniform: 1.0,
                    xyz: [1.0, 1.0, 1.0],
                },
                position_offset: [0.0, 0.0, 0.0],
            },
            camera: CameraConfig {
                cockpit: CockpitCamera {
                    position: Position {
                        xyz: [1.56, 4.78, -2.28],
                        description: "Position in model local space".to_string(),
                    },
                    rotation: Rotation {
                        quaternion: [0.0, -0.998, -0.053, 0.0],
                        euler: [0.0, 0.0, 0.0],
                        euler_unit: "degrees".to_string(),
                    },
                    fov: 75.0,
                },
            },
            objects: vec![
                SceneObject {
                    name: "BMW_Model".to_string(),
                    transform: Transform {
                        position: [0.0, 0.0, 0.0],
                        rotation_euler: [0.0, 180.0, 0.0],
                        scale: [1.0, 1.0, 1.0],
                    },
                    enabled: true,
                },
                SceneObject {
                    name: "Road_Surface".to_string(),
                    transform: Transform {
                        position: [0.0, -1.0, 0.0],
                        rotation_euler: [0.0, 0.0, 0.0],
                        scale: [10.0, 1.0, 10.0],
                    },
                    enabled: true,
                },
            ],
        }
    }
}
