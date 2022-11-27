
The Frank Engine
Copyright 2013 Frank Force
http://www.frankforce.com

Frank Engine is a game development framework with a built in editor written 
in C++. The goal is to be a fast way to create large open world 2D games with 
high quality graphics. Everything necessary to create afulld game is included: 
physics (Box2D), rendering (DirectX), level editor, dynamic lighting, particle 
system, sound, music (Ogg Vorbis), gamepad input, GUI, and debug console. The 
code is fairly well documented and includes simple starter projects build on.

================================================================================

Features

* Focus on simplicity and flexibality
* Object oriented
* Very large streamable worlds with small memory footprint
* Sub frame interpolation with fixed time step
* Integrated level editor
* Multi pass dynamic light system
* Simple and fast particle engine
* Integrated physics with Box2D
* 2d sprite and terrain rendering
* Dev environment is Microsoft Visual Studio 2017
* Graphics SDK is Microsoft DirectX 9
* Stereo sound effects with Direct Sound
* Music with Ogg Vorbis
* Keyboard, mouse, and gamepad input
* Uses XInput for gamepad and rumple
* Integrated pathfinding and minimap solution
* Integrated with The DirectX Utility Library (GUI, sound, etc)
* All assets can be compiled into the exe (besides music)
* In game debug console
* Intended to be used with all free tools
* Integrated tile sheets support with rotation and mirror support
* Custom font rendering using BMFont
* User friendly editor features including cut, copy, paste, undo, and redo

================================================================================

Engine Controls

~  - Debug Console
F1 - Show Info
F2 - Physics Debug
F3 - Refresh Resources
F4 - Edit Mode
F5 - Save Screenshot
F6 - Profiler
F7 - Light Debug
F8 - Cycle Render Pass Display
F9 - Particle Debug
Alt+Enter - Toggle full screen

================================================================================

Edit Mode Controls

Use F4 to switch bettween the editor and the game. The editor uses a WASD hand
position. Use number keys to change which layer you are editing/selecting. You
can use right mouse to select multiple tiles/objects. You can also hold alt
while selecting to select all layers. In tile edit mode you can draw
with the left mouse button. In object edit mode you can move objects and
change their attributes. Cut, copy, paste and undo/redo are all very functional.
When you swich out of edit mode back to the game the player will automatically
be moved to the mouse cursor to make iteration faster.

There are many debug console commands available to customize the editor.

Quick Preview
	Tab - While held in editor switches to displaying real time game

Layer Select
	1 – Background layer
	2 – Foreground layer
	3 – Objects layer

Camera
	Middle Mouse (or Left Mouse + Space) – Move camera
	Mouse Wheel (or Right Mouse + Space) – Zoom camera

Selection
	Mouse Right - Rectangle select
	+ Alt – multi layer select
	+ Shift – add/remove from selection (only for objects)

Rotation/Mirror Controls
	Q/E - Rotate the current selction
	F - Flip (mirror) the current selection

Tile Edit
	Mouse Left – Draw tile data & select/deselect
	+ Shift – Draw line
	+ Ctrl – Eyedropper
	+ Mouse Right – Block erase
	+ Alt – Block draw
	Z – Flood fill
	X – Flood erase

Object Edit
	Mouse Left – Object select
	+ Ctrl – Scale object (only can be done when 1 object is selected)
	+ Shift – Rotate object (only can be done when 1 object is selected)
	Z – Add object
	X – Remove object

Shortcuts
	WASD – Move in quick pick box
	Ctrl+Z, Y – Undo, Redo
	Ctrl+X, C, V – Cut, Copy Paste
	Ctrl+S, L – Save, Load
	C - Select and copy the entire patch under the mouse
	V - Paste centered on patch under the mouse
	G – Toggle grid
	B – Toggle block edit

