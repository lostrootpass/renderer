# Vulkan Renderer

A rendering sandbox project intended to accumulate rendering features over time. Currently contains a few interesting features like deferred rendering, SSAO, and omnidirectional shadow maps as well as the ability to drop-in fragment shaders for postprocessing a la Shadertoy. Future iterations will add extra features like PBR and a basic UI to control the settings.

Video
---
[Watch on Vimeo](https://vimeo.com/pmurdoch/renderer)


Building
---
Requirements:
* The [Vulkan SDK](https://vulkan.lunarg.com/) is required and `VULKAN_SDK` in your `PATH` should be set appropriately.
* [SDL 2.0.5](http://libsdl.org)
* [glm](https://glm.g-truc.net/0.9.8/index.html)
* `stb_image.h` from [Sean Barrett's header library](https://github.com/nothings/stb).
* [tinyobjloader](https://github.com/syoyo/tinyobjloader)

After modifying a shader, run the `buildshaders.ps1` script from that shader's directory.

Currently only Windows build configurations are available; there is nothing platform-specific in the codebase so Linux build configs will be available eventually.

Running
---
The first command line parameter is the name of a directory under the `assets/models/` directory, the second parameter (optional) is the scale of that model, e.g.:

`> Renderer.exe sponza 0.01`

The `.obj` file to be loaded must have the same name as the directory itself e.g. `models/sponza/sponza.obj`.

Controls
---
* `WASDQE` - move camera forward/back/left/right/up/down
	* Hold `Shift` to increase camera movement speed
	* Hold `Ctrl` to decrease camera movement speed
* `Arrow keys` - rotate camera
* `RMB (Hold)` - mouselook
* `F1` - toggle specularity
* `F2` - toggle shadows
* `F3` - toggle Percentage Closer Filtering (PCF) on shadows
* `F4` - toggle SSAO
* `F5` - flush shader cache and hot reload
* `L` - move [L]ight to current camera eyepoint
* `P` - toggle [P]relit scene
* `B` - toggle [B]ump mapping
* `M` - toggle [M]apsplit (view normals and diffuse side-by-side)
* `N` - show [N]ormals
* `R` - [R]eset camera position and orientation

License
---
The project itself is licensed under the zlib license:

```
Copyright (C) 2017-2018 Pete Murdoch <http://p-m.pm>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
```

Individual assets in this repository belong to their respective creators, see the corresponding license details in each asset folder.