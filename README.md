# Compute-Ray-Tracer

**Compute-Ray-Tracer** is a compute shader-based ray tracing engine designed to render realistic 3D scenes with GPU acceleration. This project is a work-in-progress and serves as a foundation for experimenting with ray tracing concepts.

## Core Features

* **Compute Shader Acceleration**: Leverages GPU compute cores for ray tracing.
* **Single Model Support**: Load and render one 3D model at a time.
* **Baked Material**: Uses a default material applied to the model.
* **Ambient Lighting**: Currently supports one global ambient light source.
* **Fixed Camera**: Predefined camera setup for consistent renders.
* **High-Quality Output**: Render images with realistic shading and shadows.

## Current Status & TODO

The project is actively under development. Many features are still being implemented:

* [x] Load a single 3D model (OBJ)
* [x] Render with default baked material
* [x] Ambient lighting
* [x] Fixed camera setup
* [ ] Support multiple models
* [ ] Support multiple materials
* [ ] Add movable camera
* [ ] Add multiple light sources
* [ ] Post-processing effects (e.g., tone mapping, gamma correction)
* [ ] Performance optimizations
* [ ] UI for scene configuration

## Screenshots


## Installation

Clone the repository:

```
git clone https://github.com/yourusername/Compute-Ray-Tracer.git
cd Compute-Ray-Tracer
```

Build using CMake:

```
mkdir build
cd build
cmake ..
make
```

## Usage

```
./ComputeRayTracer scene.obj output.png
```

* `scene.obj` – Path to your 3D model file.
* `output.png` – Output image file.

Optional flags:

* `--width <pixels>`: Set image width (default: 800)
* `--height <pixels>`: Set image height (default: 600)

## Project Structure

```
Compute-Ray-Tracer/
│
├─ src/                # Source code  
├─include/             # Header files  
├─shaders/             # Compute shader files  
├─models/              # Sample 3D models  
├─docs/                # Screenshots and documentation  
└─CMakeLists.txt       # Build configuration
```

## Contributing

Contributions are welcome! You can help by:

* Adding new material shaders
* Implementing movable camera
* Supporting multiple light sources
* Optimizing performance
* Adding UI and scene management features

## License

This project is licensed under the MIT License. See [LICENSE](LICENSE).

## Acknowledgements

* [TinyOBJLoader](https://github.com/syoyo/tinyobjloader) – For OBJ model loading
* Inspired by Sebastian Lague’s [Ray Tracing Series](https://github.com/SebLague/Ray-Tracing) tutorials
* Inspired by modern GPU ray tracing tutorials and research
