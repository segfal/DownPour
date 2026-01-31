use crate::scene_data::{SceneConfig, SceneObject};
use crate::renderer::{SceneRenderer, CameraMode};
use egui::{Context, SidePanel, CentralPanel, ScrollArea, DragValue, Ui, ColorImage, TextureOptions};

pub struct EditorState {
    pub config: SceneConfig,
    pub selected_object: Option<usize>,
    pub config_path: String,
    pub unsaved_changes: bool,
    pub status_message: Option<String>,

    // 3D rendering
    renderer: Option<SceneRenderer>,
    camera_mode: CameraMode,
    model_path: Option<String>,

    // Mouse state for camera controls
    last_mouse_pos: Option<egui::Pos2>,
    is_dragging_left: bool,
    is_dragging_right: bool,
}

impl EditorState {
    pub fn new(config: SceneConfig, config_path: String) -> Self {
        Self {
            config,
            selected_object: None,
            config_path,
            unsaved_changes: false,
            status_message: None,
            renderer: None,
            camera_mode: CameraMode::Orbital,
            model_path: None,
            last_mouse_pos: None,
            is_dragging_left: false,
            is_dragging_right: false,
        }
    }

    pub fn render(&mut self, ctx: &Context) {
        // Top menu bar
        egui::TopBottomPanel::top("top_panel").show(ctx, |ui| {
            egui::menu::bar(ui, |ui| {
                ui.label("DownPour Scene Editor");
                ui.separator();

                if ui.button("💾 Save").clicked() {
                    self.save();
                }

                if ui.button("🔄 Reload").clicked() {
                    self.reload();
                }

                if ui.button("📋 New Object").clicked() {
                    self.add_new_object();
                }

                if ui.button("📦 Load Model").clicked() {
                    self.open_model_browser();
                }

                ui.separator();

                if self.unsaved_changes {
                    ui.label("⚠ Unsaved changes");
                }

                if let Some(ref msg) = self.status_message {
                    ui.colored_label(egui::Color32::GREEN, msg);
                }

                if let Some(ref path) = self.model_path {
                    ui.label(format!("Model: {}",
                        std::path::Path::new(path)
                            .file_name()
                            .and_then(|n| n.to_str())
                            .unwrap_or("unknown")));
                }
            });
        });

        // Left panel: Object list
        SidePanel::left("objects_panel")
            .default_width(300.0)
            .resizable(true)
            .show(ctx, |ui| {
                ui.heading("Scene Objects");
                ui.separator();

                ScrollArea::vertical().show(ui, |ui| {
                    // Model configuration
                    ui.collapsing("Model Configuration", |ui| {
                        ui.label("Target Length:");
                        let mut target_length = self.config.model.target_length;
                        if ui.add(DragValue::new(&mut target_length).speed(0.1)).changed() {
                            self.config.model.target_length = target_length;
                            self.unsaved_changes = true;
                        }

                        ui.label("Orientation (Euler):");
                        for (i, label) in ["X", "Y", "Z"].iter().enumerate() {
                            ui.horizontal(|ui| {
                                ui.label(format!("{}:", label));
                                if ui.add(DragValue::new(&mut self.config.model.orientation.euler[i])
                                    .speed(1.0)
                                    .suffix("°")).changed() {
                                    self.unsaved_changes = true;
                                }
                            });
                        }
                    });

                    // Camera configuration
                    ui.collapsing("Camera Configuration", |ui| {
                        ui.label("Position:");
                        for (i, label) in ["X", "Y", "Z"].iter().enumerate() {
                            ui.horizontal(|ui| {
                                ui.label(format!("{}:", label));
                                if ui.add(DragValue::new(&mut self.config.camera.cockpit.position.xyz[i])
                                    .speed(0.01)).changed() {
                                    self.unsaved_changes = true;
                                }
                            });
                        }

                        ui.label("FOV:");
                        if ui.add(DragValue::new(&mut self.config.camera.cockpit.fov)
                            .speed(1.0)
                            .range(30.0..=120.0)
                            .suffix("°")).changed() {
                            self.unsaved_changes = true;
                        }
                    });

                    ui.separator();
                    ui.heading("Objects");

                    for (idx, obj) in self.config.objects.iter().enumerate() {
                        let is_selected = self.selected_object == Some(idx);

                        if ui.selectable_label(is_selected, &obj.name).clicked() {
                            self.selected_object = Some(idx);
                        }
                    }
                });
            });

        // Right panel: 3D Viewport
        SidePanel::right("viewport_panel")
            .default_width(600.0)
            .resizable(true)
            .show(ctx, |ui| {
                ui.heading("3D Viewport");
                ui.separator();

                // Camera mode toggle
                ui.horizontal(|ui| {
                    ui.label("Camera:");
                    if ui.selectable_label(self.camera_mode == CameraMode::SceneCamera, "Scene").clicked() {
                        self.camera_mode = CameraMode::SceneCamera;
                        if let Some(ref mut renderer) = self.renderer {
                            renderer.set_camera_mode(CameraMode::SceneCamera);
                            renderer.update_from_config(&self.config);
                        }
                    }
                    if ui.selectable_label(self.camera_mode == CameraMode::Orbital, "Orbital").clicked() {
                        self.camera_mode = CameraMode::Orbital;
                        if let Some(ref mut renderer) = self.renderer {
                            renderer.set_camera_mode(CameraMode::Orbital);
                        }
                    }
                });

                ui.add_space(5.0);

                // Render viewport or show placeholder
                if let Some(ref mut renderer) = self.renderer {
                    if renderer.has_model() {
                        self.render_viewport(ui, ctx);
                    } else {
                        ui.vertical_centered(|ui| {
                            ui.add_space(200.0);
                            ui.heading("No Model Loaded");
                            ui.label("Click 'Load Model' to open a GLTF/GLB file");
                        });
                    }
                } else {
                    ui.vertical_centered(|ui| {
                        ui.add_space(200.0);
                        ui.heading("Renderer Not Initialized");
                        ui.label("Click 'Load Model' to start");
                    });
                }

                // Camera controls help
                if self.camera_mode == CameraMode::Orbital {
                    ui.add_space(10.0);
                    ui.separator();
                    ui.label("Controls:");
                    ui.label("  • Left drag: Rotate");
                    ui.label("  • Right drag: Pan");
                    ui.label("  • Scroll: Zoom");
                    if ui.button("Reset Camera").clicked() {
                        if let Some(ref mut renderer) = self.renderer {
                            renderer.reset_orbital_camera();
                        }
                    }
                }
            });

        // Central panel: Transform editor
        CentralPanel::default().show(ctx, |ui| {
            if let Some(idx) = self.selected_object {
                if idx < self.config.objects.len() {
                    self.render_transform_editor(ui, idx);
                } else {
                    ui.label("Selected object index out of range");
                    self.selected_object = None;
                }
            } else {
                ui.vertical_centered(|ui| {
                    ui.add_space(100.0);
                    ui.heading("No object selected");
                    ui.label("Select an object from the left panel to edit its transform");
                });
            }
        });
    }

    fn render_transform_editor(&mut self, ui: &mut Ui, idx: usize) {
        // Clone the object name to avoid borrow issues
        let obj_name = self.config.objects[idx].name.clone();

        ui.heading(&obj_name);
        ui.separator();

        ui.add_space(10.0);

        // Position section
        ui.group(|ui| {
            ui.heading("Position");
            ui.add_space(5.0);

            ui.horizontal(|ui| {
                ui.label("X:");
                if ui.add(DragValue::new(&mut self.config.objects[idx].transform.position[0])
                    .speed(0.01)
                    .max_decimals(2)).changed() {
                    self.unsaved_changes = true;
                }
            });

            ui.horizontal(|ui| {
                ui.label("Y:");
                if ui.add(DragValue::new(&mut self.config.objects[idx].transform.position[1])
                    .speed(0.01)
                    .max_decimals(2)).changed() {
                    self.unsaved_changes = true;
                }
            });

            ui.horizontal(|ui| {
                ui.label("Z:");
                if ui.add(DragValue::new(&mut self.config.objects[idx].transform.position[2])
                    .speed(0.01)
                    .max_decimals(2)).changed() {
                    self.unsaved_changes = true;
                }
            });
        });

        ui.add_space(10.0);

        // Rotation section
        ui.group(|ui| {
            ui.heading("Rotation (Euler Angles)");
            ui.add_space(5.0);

            ui.horizontal(|ui| {
                ui.label("Yaw:  ");
                if ui.add(DragValue::new(&mut self.config.objects[idx].transform.rotation_euler[0])
                    .speed(1.0)
                    .max_decimals(2)
                    .suffix("°")).changed() {
                    self.unsaved_changes = true;
                }
            });

            ui.horizontal(|ui| {
                ui.label("Pitch:");
                if ui.add(DragValue::new(&mut self.config.objects[idx].transform.rotation_euler[1])
                    .speed(1.0)
                    .max_decimals(2)
                    .suffix("°")).changed() {
                    self.unsaved_changes = true;
                }
            });

            ui.horizontal(|ui| {
                ui.label("Roll: ");
                if ui.add(DragValue::new(&mut self.config.objects[idx].transform.rotation_euler[2])
                    .speed(1.0)
                    .max_decimals(2)
                    .suffix("°")).changed() {
                    self.unsaved_changes = true;
                }
            });
        });

        ui.add_space(10.0);

        // Scale section
        ui.group(|ui| {
            ui.heading("Scale");
            ui.add_space(5.0);

            ui.horizontal(|ui| {
                ui.label("X:");
                if ui.add(DragValue::new(&mut self.config.objects[idx].transform.scale[0])
                    .speed(0.01)
                    .max_decimals(2)).changed() {
                    self.unsaved_changes = true;
                }
            });

            ui.horizontal(|ui| {
                ui.label("Y:");
                if ui.add(DragValue::new(&mut self.config.objects[idx].transform.scale[1])
                    .speed(0.01)
                    .max_decimals(2)).changed() {
                    self.unsaved_changes = true;
                }
            });

            ui.horizontal(|ui| {
                ui.label("Z:");
                if ui.add(DragValue::new(&mut self.config.objects[idx].transform.scale[2])
                    .speed(0.01)
                    .max_decimals(2)).changed() {
                    self.unsaved_changes = true;
                }
            });
        });

        ui.add_space(20.0);

        // Enabled checkbox
        ui.horizontal(|ui| {
            if ui.checkbox(&mut self.config.objects[idx].enabled, "Enabled").changed() {
                self.unsaved_changes = true;
            }
        });

        ui.add_space(20.0);

        // Delete button
        ui.horizontal(|ui| {
            if ui.button("🗑 Delete Object").clicked() {
                self.config.objects.remove(idx);
                self.selected_object = None;
                self.unsaved_changes = true;
            }
        });
    }

    fn save(&mut self) {
        match self.config.save(&self.config_path) {
            Ok(_) => {
                self.unsaved_changes = false;
                self.status_message = Some(format!("✓ Saved to {}", self.config_path));
            }
            Err(e) => {
                self.status_message = Some(format!("✗ Save failed: {}", e));
            }
        }
    }

    fn reload(&mut self) {
        match SceneConfig::load(&self.config_path) {
            Ok(config) => {
                self.config = config;
                self.unsaved_changes = false;
                self.selected_object = None;
                self.status_message = Some("✓ Reloaded from file".to_string());
            }
            Err(e) => {
                self.status_message = Some(format!("✗ Reload failed: {}", e));
            }
        }
    }

    fn add_new_object(&mut self) {
        let new_obj = SceneObject {
            name: format!("Object_{}", self.config.objects.len() + 1),
            transform: crate::scene_data::Transform {
                position: [0.0, 0.0, 0.0],
                rotation_euler: [0.0, 0.0, 0.0],
                scale: [1.0, 1.0, 1.0],
            },
            enabled: true,
        };

        self.config.objects.push(new_obj);
        self.selected_object = Some(self.config.objects.len() - 1);
        self.unsaved_changes = true;
    }

    fn open_model_browser(&mut self) {
        // Open file dialog to select GLTF/GLB file
        if let Some(path) = rfd::FileDialog::new()
            .add_filter("GLTF Models", &["gltf", "glb"])
            .pick_file()
        {
            self.load_model(path.to_string_lossy().to_string());
        }
    }

    fn load_model(&mut self, path: String) {
        // Initialize renderer if not already done
        if self.renderer.is_none() {
            match SceneRenderer::new(800, 600) {
                Ok(renderer) => {
                    self.renderer = Some(renderer);
                }
                Err(e) => {
                    self.status_message = Some(format!("✗ Failed to create renderer: {}", e));
                    return;
                }
            }
        }

        // Load model
        if let Some(ref mut renderer) = self.renderer {
            match renderer.load_model(&path) {
                Ok(_) => {
                    self.model_path = Some(path.clone());
                    renderer.set_camera_mode(self.camera_mode);
                    renderer.update_from_config(&self.config);
                    self.status_message = Some(format!("✓ Loaded model: {}",
                        std::path::Path::new(&path)
                            .file_name()
                            .and_then(|n| n.to_str())
                            .unwrap_or("unknown")));
                }
                Err(e) => {
                    self.status_message = Some(format!("✗ Failed to load model: {}", e));
                }
            }
        }
    }

    fn render_viewport(&mut self, ui: &mut Ui, ctx: &Context) {
        let available_size = ui.available_size();
        let width = available_size.x as u32;
        let height = (available_size.y - 100.0).max(400.0) as u32;

        // Resize renderer if needed
        if let Some(ref mut renderer) = self.renderer {
            renderer.resize(width, height);

            // Render scene
            match renderer.render() {
                Ok(pixels) => {
                    // Convert pixels to egui texture
                    let color_image = ColorImage::from_rgba_unmultiplied(
                        [width as usize, height as usize],
                        &pixels,
                    );

                    let texture = ctx.load_texture(
                        "viewport",
                        color_image,
                        TextureOptions::default(),
                    );

                    // Display image and handle input
                    let response = ui.add(
                        egui::Image::new(&texture)
                            .fit_to_exact_size(egui::vec2(width as f32, height as f32))
                            .sense(egui::Sense::click_and_drag())
                    );

                    // Handle mouse input
                    if let Some(ref mut renderer) = self.renderer {
                        // Track mouse dragging
                        if response.dragged_by(egui::PointerButton::Primary) {
                            let delta = response.drag_delta();
                            renderer.handle_mouse_drag(delta.x, delta.y, "left");
                        }

                        if response.dragged_by(egui::PointerButton::Secondary) {
                            let delta = response.drag_delta();
                            renderer.handle_mouse_drag(delta.x, delta.y, "right");
                        }

                        // Handle scroll
                        if response.hovered() {
                            let scroll_delta = ui.input(|i| i.raw_scroll_delta.y);
                            if scroll_delta != 0.0 {
                                renderer.handle_mouse_scroll(scroll_delta * 0.01);
                            }
                        }
                    }

                    // Request repaint for continuous updates
                    ctx.request_repaint();
                }
                Err(e) => {
                    ui.label(format!("Render error: {}", e));
                }
            }
        }
    }
}
