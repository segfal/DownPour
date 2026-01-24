import argparse
import trimesh
from pathlib import Path
import numpy as np


def export_preview_image(scene, output_path: Path, resolution=(1920, 1080)):
    """
    Export a preview image of the scene
    
    Args:
        scene: The trimesh Scene object
        output_path: Path to save the image
        resolution: Image resolution tuple (width, height)
    """
    try:
        print(f"Generating preview image at {resolution[0]}x{resolution[1]}...")
        
        # Save a PNG of the scene
        png_data = scene.save_image(resolution=resolution, visible=True)
        
        if png_data is not None:
            with open(output_path, 'wb') as f:
                f.write(png_data)
            print(f"Preview saved to: {output_path}")
            return True
        else:
            print("Warning: Could not generate preview image")
            return False
    except Exception as e:
        print(f"Error generating preview: {e}")
        return False


def load_and_render_gltf(gltf_path: Path, show_info: bool = True, export_preview: bool = False):
    """
    Load and render a GLTF file in 3D viewer
    
    Args:
        gltf_path: Path to the GLTF file
        show_info: Whether to print mesh information
    """
    print(f"Loading GLTF file: {gltf_path}")
    
    if not gltf_path.exists():
        print(f"Error: File not found at {gltf_path}")
        return
    
    # Load the GLTF file
    try:
        scene = trimesh.load(str(gltf_path), force='scene')
    except Exception as e:
        print(f"Error loading GLTF file: {e}")
        return
    
    # Check scene bounds
    if isinstance(scene, trimesh.Scene):
        try:
            bounds = scene.bounds
            extents = scene.extents
            scale = scene.scale
            print(f"\nScene Bounds:")
            print(f"  Min: [{bounds[0][0]:.2f}, {bounds[0][1]:.2f}, {bounds[0][2]:.2f}]")
            print(f"  Max: [{bounds[1][0]:.2f}, {bounds[1][1]:.2f}, {bounds[1][2]:.2f}]")
            print(f"  Extents: [{extents[0]:.2f}, {extents[1]:.2f}, {extents[2]:.2f}]")
            print(f"  Scale: {scale:.2f}")
        except Exception as e:
            print(f"Warning: Could not compute scene bounds: {e}")
    
    # Display information about the scene
    if show_info:
        print("\n" + "="*60)
        print("GLTF Scene Information")
        print("="*60)
        
        if isinstance(scene, trimesh.Scene):
            print(f"Scene contains {len(scene.geometry)} meshes")
            print(f"\nMeshes:")
            
            total_vertices = 0
            total_faces = 0
            
            for name, geometry in scene.geometry.items():
                vertices = len(geometry.vertices)
                faces = len(geometry.faces)
                total_vertices += vertices
                total_faces += faces
                
                print(f"\n  {name}:")
                print(f"    Vertices: {vertices:,}")
                print(f"    Faces: {faces:,}")
                
                # Check for materials
                if hasattr(geometry, 'visual') and geometry.visual:
                    if hasattr(geometry.visual, 'material'):
                        material = geometry.visual.material
                        print(f"    Material: {material.name if hasattr(material, 'name') else 'Unnamed'}")
            
            print(f"\n{'='*60}")
            print(f"Total Vertices: {total_vertices:,}")
            print(f"Total Faces: {total_faces:,}")
            print(f"{'='*60}\n")
        else:
            # Single mesh
            print(f"Single mesh with {len(scene.vertices):,} vertices and {len(scene.faces):,} faces")
    
    # Export preview image if requested
    if export_preview:
        preview_path = gltf_path.parent / f"{gltf_path.stem}_preview.png"
        export_preview_image(scene, preview_path)
        print()
    
    # Render the scene in 3D viewer
    print("\nOpening 3D viewer...")
    print("Controls:")
    print("  - Left click + drag: Rotate")
    print("  - Right click + drag: Pan")
    print("  - Scroll: Zoom")
    print("  - Press 'w' for wireframe mode")
    print("  - Press 'z' to reset view")
    print("  - Press 'h' for help")
    print("  - Press 'q' or close window to exit\n")
    
    try:
        # Configure viewer settings for better visibility
        viewer_config = {
            'start_loop': True,
            'smooth': True,
            'flags': {
                'cull': False,  # Don't cull backfaces
            }
        }
        
        # Show the scene
        scene.show(**viewer_config)
    except Exception as e:
        print(f"Error rendering scene: {e}")
        print("\nTrying basic rendering method...")
        try:
            # Try basic show if advanced fails
            scene.show()
        except Exception as e2:
            print(f"Alternative rendering also failed: {e2}")
            print("Note: 3D viewer requires a display.")


def main():
    parser = argparse.ArgumentParser(
        description="Load and render GLTF files in 3D",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s                          # Load default BMW car model
  %(prog)s models/scene.gltf       # Load specific GLTF file
  %(prog)s path/to/model.gltf      # Load any GLTF file
        """
    )
    
    parser.add_argument(
        'gltf_file',
        nargs='?',
        help='Path to GLTF file (default: models/bmw.gltf)'
    )
    
    parser.add_argument(
        '--no-info',
        action='store_true',
        help='Skip printing mesh information'
    )
    
    parser.add_argument(
        '--preview',
        action='store_true',
        help='Export a preview image before opening viewer'
    )
    
    args = parser.parse_args()
    
    # Determine which file to load
    if args.gltf_file:
        gltf_path = Path(args.gltf_file)
        # If relative path, make it relative to script directory
        if not gltf_path.is_absolute():
            gltf_path = Path(__file__).parent / gltf_path
    else:
        # Default to BMW car
        gltf_path = Path(__file__).parent / "models" / "bmw.gltf"
    
    # Load and render
    load_and_render_gltf(gltf_path, show_info=not args.no_info, export_preview=args.preview)


if __name__ == "__main__":
    main()
