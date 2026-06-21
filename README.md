# Art3D Engine Core (C++)

A lightweight C++20 game engine viewport built using SDL3 and Dear ImGui (Docking branch).

## Features
- **Dear ImGui (Docking):** Full Dockspace layout with hierarchical Scene Explorer, Properties Inspector, and Console.
- **SDL3 GPU API:** DXIL-based rendering pipeline with forward lighting (point, spot, directional).
- **ArtScript:** Custom JavaScript-like scripting via QuickJS with autocomplete, export registry, and inline execution.
- **Scene Editing:** 3D gizmo tools (Move/Rotate/Scale), OBJ model import, undo/redo history.
- **FPS Limiter:** Frame rate locked at 120 FPS.

## Project Structure
```
engine/
  core/         Math types, matrix, raycast, main Application class
  render/       GPU pipeline, renderer, OBJ loader, light types
  scene/        GameObjects, lights, parts, scripts, scene utilities
  editor/       ImGui windows (Explorer, Properties, Console, Script Editor)
  camera/       First-person fly camera controller
  ui/           2D drawable base classes and Frame
  script/       QuickJS wrapper, ScriptManager, ArtScript preprocessing
  shaders/      HLSL source and compiled DXIL shaders
assets/         OBJ model files
imgui/          Dear ImGui sources (docking branch)
include/        SDL3 and third-party headers
```

## Setup & Build
1. Open `Art3DC++.slnx` in Visual Studio 2022+ or open the folder in VS Code with MSBuild.
2. Build with MSBuild or press `Ctrl+Shift+B` in VS Code.
3. Run the executable (requires SDL3 DLLs from `art.libs/x64/` in the output directory).
