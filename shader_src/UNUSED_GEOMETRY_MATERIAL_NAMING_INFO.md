# Geometry Material naming information.

The naming convention is as below:
```
gm.XXXXXXXX.NAME.(vert/frag)
```
NOTE: A material must be the same name for the vertex and fragment shader.


## Parts of naming.

First, it starts with `gm.` showing it's a "geometry material".

Then, there is a 32-bit hexcode `XXXXXXXX`, where each digit is `0-F`. See [hexcode settings](#hexcode-settings) for info on what all the digits mean.

Then, there is `NAME` which is simply the name of the shader. This is used for lookup and matching later.

Then, the extension (`vert` or `frag`). This should be self-explanatory, but pairs of vertex and fragment shaders are searched.


## Hexcode settings.

## Script to edit geometry materials.

To make this easier, use the script below to edit geometry material hexcode settings:
```
python ./edit_geommat_settings.py NAME
```
Where `NAME` is the `NAME` part of the material. If naming for the first time, set `XXXXXXXX` to `00000000`. If a pair of validly named geom mats aren't found, then the script reports the error. If the hexcode settings do not match, then a warning is reported and the vertex shader one is loaded.

The script is interactive so you can add or remove settings and then save upon exiting.
