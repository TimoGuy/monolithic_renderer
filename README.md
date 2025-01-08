# Monolithic Renderer

A renderer that owns everything it needs.

In the case of a Windows renderer, this renderer owns GPU driver access and windowing.


## Build Steps

First, initialize submodules and generate cmake project.
```
git submodule update --init
mkdir build
cd build
cmake ..
```

Then, open up the generated project and build.
