# Tools

Command-line utilities for building and processing engine assets.

---

## AssetBuilder

Batch asset processor. Recursively walks an input directory, applies the appropriate processor for each recognised file type, and writes the result to an output directory preserving the relative path structure. Unrecognised files are copied verbatim. Processing is skipped for any output file that is already up-to-date (output timestamp ≥ input timestamp).

```
AssetBuilder <input_dir> <output_dir> [--quiet]
```

| Argument | Description |
|---|---|
| `input_dir` | Root of the source asset tree |
| `output_dir` | Root of the processed asset tree |
| `--quiet` | Suppress per-file progress output |

### Supported file types

| Extension(s) | Processor | Output |
|---|---|---|
| `.png` `.jpg` `.jpeg` | `TextureProcessor` | `.ktx2` |
| `.glsl` `.vs` `.fs` `.gs` `.ts` `.vert` `.frag` `.wgsl` | `ShaderAssetProcessor` | same extension |
| anything else | verbatim copy | same |

### Settings files

Processor behaviour can be configured per-directory or per-file using YAML files placed alongside the assets. Settings are inherited from parent directories and can be overridden at any level.

**Directory-wide defaults** — place a settings file in any directory; it applies to all assets in that directory and all subdirectories recursively:

| File | Applies to |
|---|---|
| `texture.yaml` | All texture files in the directory tree |
| `shader.yaml` | All shader files in the directory tree (reserved for future options) |

**Per-file override** — override settings for a single file by placing a `<filename>.<ext>.yaml` file next to it:

```
assets/
  textures/
    texture.yaml          ← defaults for all textures in this subtree
    diffuse.png
    diffuse.png.yaml      ← overrides for diffuse.png only
    normals/
      texture.yaml        ← overrides defaults for this sub-directory
      normal_map.png
```

**Texture settings** (`texture.yaml`):

| Key | Type | Default | Description |
|---|---|---|---|
| `build_mips` | bool | `true` | Generate a full mip chain |
| `linear_mips` | bool | `false` | Use simple linear averaging for mip filtering instead of gamma-correct sRGB-aware filtering |

---

## png2ktx

Converts a single PNG or JPEG image to KTX2 format with optional mip generation.

```
png2ktx <input.[png|jpg|jpeg]> <output.ktx2> [--quiet] [--linear]
```

| Flag | Description |
|---|---|
| `--quiet` | Suppress progress output |
| `--linear` | Use linear averaging for mip filtering (default: gamma-correct sRGB-aware) |

---

## ShaderPreprocessor

Resolves `#include` directives in GLSL / WGSL shader files, producing a single fully-inlined output file. `#pragma once` is respected across the include graph. `// Preprocessor: Ignore` on an `#include` line passes it through verbatim.

```
ShaderPreprocessor <input_shader> <output_shader> [base_directory]
```

| Argument | Description |
|---|---|
| `input_shader` | Shader file to preprocess |
| `output_shader` | Output file path (directory is created if needed) |
| `base_directory` | Fallback search path for includes that cannot be resolved relative to the including file (defaults to the input file's directory) |

---

## Architecture

All three tools share a common `AssetProcessor` base class defined in `asset_processor.hpp`. `AssetBuilder` discovers processors at runtime and calls them via the virtual interface.

```
AssetProcessor  (abstract)
├── TextureProcessor       — PNG/JPEG → KTX2  (texture_processor.hpp/.cpp)
└── ShaderAssetProcessor   — GLSL/WGSL inliner (shader_processor.hpp/.cpp)
       uses ShaderPreprocessor internally
```

Each processor implements a settings stack (`PushSettings` / `PopSettings`) so that `AssetBuilder` can push directory-level and per-file settings onto the stack as it descends the directory tree and pop them on the way back up, ensuring correct inheritance without leaking state across sibling directories.
