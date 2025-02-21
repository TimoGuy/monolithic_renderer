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
    - Perhaps... use amd brixelizer gi? It is screen space GI but that could be good. It would probably dictate that the application be deferred rendering tho.
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


## Coordination with world entities.

- Receive updates over time from simulation job.
    - Tick idx and the transform info (vec3 pos, quat rot, vec3 scale).
    - Tick idx is given so that interpolation scale is known between transforms. Further away updates are given so that computation time decreases with each simulation step and timeslicing is used.
        - @NOTE: if the buffer gets written to all the time is interpolation gonna just be turned off for far away things too then????
    - Triple buffer transform info to ensure no need for contentious access of most recent buffer.
- Write render information.
    - Use 2nd and 3rd buffer info to interpolate transform if interpolation enabled.
    - Write new transform into the rendering transform buffer.
- Render out all the render objects.
    - Use the transform buffer to do this.
