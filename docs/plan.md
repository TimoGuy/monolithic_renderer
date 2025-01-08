# Plan for first version.

## All-in-one connection to the media.

Just a simple construction of the `Monolithic_renderer` and it will create the necessary renderer and presentation structure!

... Is the plan.

So that looks like essentially being GLFW + Vulkan for Windows, GLFW + MoltenVK for MacOS (in the future), and whatever-the-heck-consoles-use-hahaha for Consoles (... in the future).


## Some simple customization of the render pipeline.

Since this is the monolithic renderer, a rendering pipeline designed for the 3D games you're wanting to make.
- Material sets that connect to shaders
- BRDF
- Irradiance and Reflectance maps
- Shadows
- "Outdoor influence" occlusion system
- Volumetric rendering
    - Clouuuuuuuds!!!!
    - God rays!!!
    - Smoke effects
    - @NOTE: maybe having all of the localized volumetric effects be bound in a mathematical SDF?
- Decal rendering
- LODs
- Ambient Occlusion
- Screen-space Reflections/Refractions

> @THOUGHT: I wonder if we could just make this a deferred renderer and then do a bunch of light culling and decal tricks on it. I wonder if we could do importance sampling with lights stochastically? I wonder if we can get the same effects without having to use ray tracing? Maybe discretizing the scene into a hierarchical SDF? Doing that would likely make it a lot easier to do global illumination or at the very least a skylight influence calculation.

On top of that basic rendering pipeline is a number of add-on effects that can be enabled:
- Volumetric rendering
- Decal rendering
- 
- Screen Space Reflections
- Ambient Occlusion
- Post-processing effects


## Unaware of jobification.

Important things like construction of the renderer is not thread-safe, so manual handling of atomics and/or locks will have to be done on the implementing program side. 
