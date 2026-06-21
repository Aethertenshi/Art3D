# Art3DC++ Rendering Pipeline Cleanup Report

## Changes Made

### 1. Removed Dead Shadow Code

**Files modified:**
- `Art3DC++.cpp` — Removed entire shadow pass block (~140 lines):
  - Shadow light selection loop
  - Shadow pass rendering (depth-only pass)
  - Shadow map clearing pass
  - Shadow-related uniform fields (`shadowActive`, `shadowLightIndex`, `lightVP`)
  - `shadowActive`, `shadowLight`, `shadowLightIdx` variables
- `GpuPipeline.h` — Removed all shadow infrastructure:
  - `DepthPipeline` member
  - `ShadowMap`, `ShadowDummyColor`, `ShadowSampler` textures
  - `LightVP` matrix
  - `SHADOW_MAP_SIZE` constant
  - `CreateShadowResources()` and `ReleaseShadowResources()` functions
  - Depth-only pipeline creation block (~80 lines)
- `shaders/depth_only.hlsl` — deleted
- `shaders/depth_only.vs.dxil` — deleted
- `shaders/depth_only.ps.dxil` — deleted

### 2. Fixed Uniform Push Bugs

**Before:** Fragment uniforms (LightUBO) were only pushed if `shadowActive == true`. When disabled, the grid, objects, and gizmos had no LightUBO data, causing stale-data reads in the shader.

**After:** Fragment uniforms are always pushed for:
- Grid rendering
- All 3D objects
- Gizmo handles

### 3. Reorganized Shaders to New Module Directory

**New location:** `art.modules/art.rendering/shaders/`
- `shader.hlsl` — cleaned version without shadow code
- `shader.vs.dxil` — recompiled vertex shader
- `shader.ps.dxil` — recompiled fragment shader
- `compile_shaders.ps1` — updated script

**Shader cleanup:**
- Removed `ShadowMap` texture declaration
- Removed `ShadowSampler` sampler declaration
- Removed `CalcShadow()` function
- Removed `ShadowActive`, `ShadowLightIndex`, `LightVP` from `LightUBO`
- Removed shadow factor computation from `PSMain`
- Removed per-light shadow multiplication in the lighting loop

**Pipeline update:**
- `num_samplers` changed from `1` to `0` (no shadow sampler needed)
- Shader search paths updated to `art.modules/art.rendering/shaders/`

### 4. LightUBO Struct Simplified

**Before (592 bytes):**
```
GPULight lights[8];      // 512 bytes
int lightCount;          // 4 bytes
float ambientIntensity;  // 4 bytes
int shadowActive;        // 4 bytes
int shadowLightIndex;    // 4 bytes
Matrix4x4 lightVP;       // 64 bytes
```

**After (520 bytes):**
```
GPULight lights[8];      // 512 bytes
int lightCount;          // 4 bytes
float ambientIntensity;  // 4 bytes
```

### 5. LookAt Cross-Product Fix (from previous session)

Fixed `Matrix4x4::LookAt()` in `art.modules/art.datatypes/Matrix4x4.h`:
- Changed `s = cross(f, up)` to `s = cross(up, f)` (left-handed right vector)
- Changed `u = cross(s, f)` to `u = cross(f, s)` (left-handed up vector)

## Build Instructions

1. Open `Art3DC++.vcxproj` in Visual Studio 2022
2. Build the project
3. Run `art.modules/art.rendering/shaders/compile_shaders.ps1` if shaders need recompilation

## Remaining Legacy Code

- `bool isShadowPass` parameter in `Render3DObject()` — defaults to `false`, harmless
- `shaders/shader.hlsl` in old location — keep as reference only
