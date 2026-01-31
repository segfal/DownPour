mod scene_data;
mod ui;
mod renderer;

use eframe::{NativeOptions, egui::ViewportBuilder};
use scene_data::SceneConfig;
use ui::EditorState;
use std::env;

struct EditorApp {
    state: EditorState,
}

impl EditorApp {
    fn new(config: SceneConfig, config_path: String) -> Self {
        Self {
            state: EditorState::new(config, config_path),
        }
    }
}

impl eframe::App for EditorApp {
    fn update(&mut self, ctx: &egui::Context, _frame: &mut eframe::Frame) {
        self.state.render(ctx);
    }
}

fn main() -> Result<(), eframe::Error> {
    // Parse command line arguments
    let args: Vec<String> = env::args().collect();

    let (config, config_path) = if args.len() > 1 {
        let path = &args[1];
        match SceneConfig::load(path) {
            Ok(cfg) => {
                println!("Loaded scene configuration from: {}", path);
                (cfg, path.clone())
            }
            Err(e) => {
                eprintln!("Error loading config from {}: {}", path, e);
                eprintln!("Using example configuration instead.");
                (SceneConfig::example(), "scene_config.json".to_string())
            }
        }
    } else {
        println!("No config file specified, using example configuration.");
        println!("Usage: {} [config.json]", args.get(0).unwrap_or(&"scene_editor".to_string()));
        (SceneConfig::example(), "scene_config.json".to_string())
    };

    let options = NativeOptions {
        viewport: ViewportBuilder::default()
            .with_inner_size([1400.0, 900.0])
            .with_title("DownPour Scene Editor"),
        ..Default::default()
    };

    eframe::run_native(
        "DownPour Scene Editor",
        options,
        Box::new(|_cc| {
            Ok(Box::new(EditorApp::new(config, config_path)))
        }),
    )
}
