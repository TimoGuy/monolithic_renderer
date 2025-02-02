# Monolithic Renderer

A renderer that owns everything it needs.

In the case of a Windows renderer, this renderer owns GPU driver access and windowing.


## Plan.

- Things to consider first:
- [ ] Will there be day-night cycle?
- [ ] Will GI be dynamic?
    - [ ] How dynamic?
- [ ] 

- Minimum Required.
- [ ] Whole render graph barebones stuff.
    - [ ] Atmospheric scattering skybox amortization.
    - [ ] Vertex transformations via compute with dynamic meshes.
        - [ ] Skinning.
        - [ ] Fabric animations.
    - [ ] Pre-do
        - [ ] Frustum and Occlusion Culling
        - [ ] Z prepass.
        - [ ] Light culling.
    - [ ] PBR renderer.
        - [ ] Shadow pass.
        - [ ] Opaque pass.
        - [ ] Water pass.
        - [ ] Transparent pass.
    - [ ] Volumetric renderer.
        - [ ] Cloud renderer.
        - [ ] Fog rendering.
        - [ ] Volumetric lighting.
    - [ ] Particle renderer.
        - [ ] 
    - [ ] World AO
        - [ ] SDF baker for individual meshes.
        - [ ] Stochastic occlusion sampling.
    - [ ] UI renderer.

- Future stuff.
- [ ] Hot swappable shaders.
- [ ] Water refraction.
- [ ] Reflections.
- [ ] TAA.
- [ ] FSR 3.1
- [ ] Global illumination (of... some kind???? Unknown how atm).


## Build Steps

First, initialize submodules and generate cmake project.
```
git submodule update --init
mkdir build
cd build
cmake ..
```

Then, open up the generated project and build.
