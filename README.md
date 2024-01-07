# Multithreaded Ray Tracer

This is a simple multi-threaded ray tracer built by following TheCherno's tutorial for raytracing up to episode 14 (https://www.youtube.com/@TheCherno) as well as Peter Shirley's `Ray Tracing in One Weekend`. It is built on top of the Dear ImGui application interface library for ease of output to the viewport and implements multithreading for great improvements to performance. The rest of the raytracer is just maths.

## Camera controls

Hold right click to engage the camera, and then use WASDEQ for forward-backward-left-right-up-down for camera movement, or drag the mouse to change the viewing angle.

## Running the project
- Requires Vulkan SDK.
- `git clone` the repository and open `MTRT.sln`
- Run the project in release mode for best performance.
