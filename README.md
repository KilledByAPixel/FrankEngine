# The Frank Engine

Frank Engine is a game development framework with a built in editor written in C++. The goal is to be a fast way to create large open world 2D games with high quality graphics. Everything necessary to create a fully featured game is included: physics (Box2D), rendering (DirectX), level editor, dynamic lighting, particle system, sound, music (Ogg Vorbis), gamepad input, GUI, and debug console. The code is fairly well documented and includes simple starter projects build on.

## [Play the demo in your browser](https://killedbyapixel.github.io/FrankEngine/)

Frank Engine compiles to WebAssembly, so games built with it run in a browser with no plugins or installs. That link is the included TestGame sample, built straight from this repo. See [Web Builds](#web-builds) below.

## Features
- Large streamable worlds with small memory footprint
- Integrated level editor, instantly switch from editing to playing!
- Integrated Box2d Physics
- Deferred rendering with diffuse, normal, specular and emissive maps
- Supports a ton of dynamic shadow casters and shadow casting lights
- 2D sprite, terrain, and particle system
- Stereo sound effects with Direct Sound
- Music with Ogg Vorbis
- Integrated with The DirectX Utility Library (GUI, sound, etc)
- Sub frame interpolation using fixed time step
- Keyboard, mouse, and joystick input binding system
- In game debug console
- User friendly editor features: cut, copy, paste, undo, and redo
- Also compiles to WebAssembly to run in a browser, rendering through WebGL 2


## Videos

### [Engine Demo](https://www.youtube.com/watch?v=lA8xqr14QIY) - Showing features of the engine
### [Piroot Trailer](https://www.youtube.com/watch?v=m9VMf790ld8) - Physics based metroidvania
### [Bulkhead Demo](https://www.youtube.com/watch?v=bAQeQzmu1mA) - Top down roguelike shooter

## Screenshots

![Screenshot 2](/screenshots/2.jpg)
![Screenshot 3](/screenshots/3.jpg)
![Screenshot 4](/screenshots/1.jpg)

## Web Builds

The same source builds for the web with [Emscripten](https://emscripten.org/), targeting WebAssembly and WebGL 2. Windows remains the primary target and is unaffected: the platform layer lives in `FrankEngine/Source/Web` and is selected with `FRANK_PLATFORM_WEB`, so the gameplay, physics and rendering code is shared.

To build the TestGame sample, install Emscripten and run:

    web/engineBuild/build.ps1

Then serve the `web` folder over HTTP and open `engineBuild/` (the .wasm and preloaded data will not load from a `file://` URL).

The deferred renderer runs in full on the web, including dynamic shadow casting lights, normal and specular mapping, terrain streaming, sound, music and gamepad input. The level editor is Windows only.

## Basic Engine Commands

These commands work by default in all games made using Frank Engine. In some games you will need to type "devmode 1" in the debug console to enable all the commands.

Tilde - Brings up the debug console where you can enter debug commands.  Try typing "help" for more info.  There are a slew of commands, everything from vsync to tweaks for specific gameplay variables.
F1 - Shows debug information like frame rate and engine stats.
F2 - Shows an Box2D debug physics overlay.
F3 - Reloads textures and sounds from disk.
F4 - Toggles edit mode, allowing a designer to create the terrain and place objects.
F5 - Takes a screenshot and saves it to the snapshots folder.
F6 - Opens up the profiler to help track where frame time is being spent.

## Editor Help

Pressing F4 will toggle between the editor and the game. Whenever you switch back to the game, the player will automatically be moved to the mouse cursor to make iteration faster.

The top right shows which layer you are editing/selecting, click on a layer number to change it, or use the number keys. On a tile layer you can draw and change tiles for visual and collision. On an object layer you can move objects and change their attributes. Select the tile or object type to draw in the pick box in the lower right. Cut, copy, paste and undo/redo are all very functional.

Draw tiles and select objects using the left mouse button. Drag right mouse button to select multiple tiles/objects. By default this will select all layers but you can hold alt to select only the active layer.

### Layer Select
- 1 - Background layer
- 2 - Foreground layer
- 3 - Objects layer

### Camera
- Middle Mouse (or Left Mouse + Tab) - Move camera
- Mouse Wheel (or Right Mouse + Tab) - Zoom camera

### Selection
- Mouse Right - Rectangle select
- Mouse Right + Alt - Single layer select
- Mouse Right + Shift - add to selection (only for objects)
- Mouse Right + Ctrl - remove from selection (only for objects)

### Tile Edit
- Mouse Left - Draw tile data & select/deselect
- Mouse Left + Shift - Draw line
- Mouse Left + Ctrl - Eyedropper
- Mouse Left + Mouse Right - Block erase
- Alt - Block draw
- Z - Flood fill
- X - Flood erase

### Object Edit
- Mouse Left - Object select
- Mouse Left + Ctrl - Scale object (only can be done when 1 object is selected)
- Mouse Left + Shift - Rotate object (only can be done when 1 object is selected)
- Z - Add object
- X - Remove object

### Shortcuts
- WASD - Move in quick pick box
- Ctrl+Z, Y - Undo, Redo
- Ctrl+X, C, V - Cut, Copy Paste
- Ctrl+S, L - Save, Load
- Shift+Ctrl+C - Copy entire active patch
- Shift+Ctrl+V - Paste entire active patch
- G - Toggle grid
- B - Toggle block edit
