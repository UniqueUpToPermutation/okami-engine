# PNG to KTX2 Converter Tool

A standalone command-line tool that converts PNG images to KTX2 format with automatically generated mipmaps.

## Overview

This tool was created as a workaround for building the KTX software tools, which can be difficult to compile on Windows. It provides a simple way to convert PNG files to KTX2 format with full mipmap chains using the KTX library's runtime API.

## Features

- Converts PNG images (with alpha channel) to KTX2 format
- Automatically generates complete mipmap pyramid
- **Gamma-correct mipmap generation** - properly handles sRGB color space
- Uses box filter with sRGB↔linear conversion for proper averaging
- Outputs uncompressed RGBA8 format (VK_FORMAT_R8G8B8A8_UNORM)
- Simple command-line interface
- Quiet mode for cleaner build output

## Building

The tool is built automatically with the okami-engine project:

```bash
cmake --build out/build/x64-debug --config Debug --target png2ktx
```

The executable will be located at:
- Windows: `out/build/x64-debug/Debug/png2ktx.exe`
- macOS/Linux: `out/build/x64-debug/png2ktx`

## Usage

```bash
png2ktx input.png output.ktx2 [--quiet] [--debug] [--linear]
```

### Options

- `--quiet` - Suppress informational output (errors still displayed)
- `--debug` - Save each mipmap level as a separate PNG file for inspection
- `--linear` - Use simple linear blending instead of gamma-correct blending (faster but produces darker mipmaps)

Options can be combined, e.g., `--debug --linear` or `--quiet --linear`

### Example

```bash
# Convert a texture with mipmaps (verbose)
png2ktx assets/test.png assets/test.ktx2

# Convert quietly (for build scripts)
png2ktx assets/test.png assets/test.ktx2 --quiet

# Debug mode - save each mipmap level as PNG for inspection
png2ktx assets/test.png assets/test.ktx2 --debug
# Creates: test_mip0.png, test_mip1.png, test_mip2.png, etc.

# Linear mode - use simple blending (no gamma correction)
png2ktx assets/normalmap.png assets/normalmap.ktx2 --linear
# Useful for normal maps, height maps, or data textures

# Combine flags - debug output with linear blending
png2ktx assets/test.png assets/test.ktx2 --debug --linear

# The tool will:
# 1. Load the PNG file
# 2. Calculate the number of mip levels (based on image dimensions)
# 3. Generate each mip level using gamma-correct box filter
# 4. Write the complete texture to KTX2 format
```

### Output

The tool provides verbose output showing the conversion process:

```
Loading PNG: assets/test.png
Image loaded: 128x128
Generating 8 mip levels...
  Level 0: 128x128
  Level 1: 64x64
  Level 2: 32x32
  Level 3: 16x16
  Level 4: 8x8
  Level 5: 4x4
  Level 6: 2x2
  Level 7: 1x1
Writing KTX2 file: assets/test.ktx2
Successfully created assets/test.ktx2
```

## Technical Details

### Format
- **Vulkan Format**: VK_FORMAT_R8G8B8A8_UNORM (format code 37)
- **Color Space**: sRGB (PNG files are assumed to be in sRGB color space)
- **Channels**: RGBA (4 channels, 8 bits per channel)
- **Compression**: None (uncompressed)

### Mipmap Generation
- Uses gamma-correct box filter (2x2 average)
- **Proper sRGB handling**: Converts to linear space before averaging, then back to sRGB
- RGB channels: sRGB → linear → average → sRGB (gamma 2.4)
- Alpha channel: Linear averaging (no gamma correction needed)
- Generates mipmaps down to 1x1 pixel
- Number of levels: `floor(log2(max(width, height))) + 1`

**Why gamma correction matters**: Averaging colors in sRGB space (without linearization) produces darker results than correct. For example, averaging 50% gray (128,128,128) in sRGB gives ~64, but correctly averaging in linear space gives ~128. This is especially important for albedo/color textures.

**When to use `--linear`**: 
- **Normal maps**: Already in linear space, don't need gamma correction
- **Height maps**: Linear data, not sRGB colors
- **Metalness/roughness maps**: Linear PBR data
- **Masks**: Binary or linear data
- **Performance testing**: Linear blending is faster (no pow() operations)

**Always use default (gamma-correct) for**:
- Albedo/diffuse textures
- Emissive textures  
- Any texture that represents actual colors in sRGB space

## Debugging Mipmaps

The `--debug` flag allows you to visually inspect each mipmap level:

```bash
png2ktx input.png output.ktx2 --debug
```

This will create PNG files for each mipmap level:
- `output_mip0.png` - Full resolution (original)
- `output_mip1.png` - Half resolution
- `output_mip2.png` - Quarter resolution
- ...and so on down to 1x1

**Use cases:**
- Verify gamma correction is working properly (mipmaps shouldn't be too dark)
- Check for aliasing artifacts
- Inspect how textures will look at different distances
- Compare with/without gamma correction to see the difference
- Debug texture quality issues

**Example workflow:**
```bash
# Generate debug mipmaps with gamma correction
png2ktx texture.png texture.ktx2 --debug

# Generate debug mipmaps without gamma correction for comparison
png2ktx texture.png texture_linear.ktx2 --debug --linear

# Open the mipmap PNGs in an image viewer
# Compare texture_mip1.png vs texture_linear_mip1.png
# The linear version will appear darker
```

**Comparison tip**: The gamma-correct mipmaps should maintain similar brightness to the original, while linear mipmaps will progressively get darker. For a 50% gray texture, level 1 should still appear close to 50% gray with gamma correction, but will appear ~25% gray with linear blending.

### Limitations
- Only supports PNG input files
- Only outputs uncompressed RGBA8 format
- No compression options (Basis Universal, etc.)
- No support for texture arrays or cube maps
- Box filter only (no advanced filtering options)

## Future Enhancements

Potential improvements that could be added:

1. **Compression support**: Add Basis Universal or ASTC compression
2. **Better filtering**: Implement Lanczos or Mitchell-Netravali filters
3. **Multiple formats**: Support different input formats (JPEG, TGA, etc.)
4. **Gamma correction**: Proper linear/sRGB conversion during mipmap generation
5. **Format options**: Allow selecting different output formats (RGB8, BC7, etc.)
6. **Batch processing**: Convert multiple files at once
7. **Quality options**: Control mipmap generation quality

## Dependencies

- **lodepng**: PNG loading (included in project)
- **KTX library**: KTX2 file writing (via vcpkg)

## See Also

- [KTX File Format Specification](https://github.khronos.org/KTX-Specification/)
- [Khronos KTX Software](https://github.com/KhronosGroup/KTX-Software)
- [lodepng library](https://github.com/lvandeve/lodepng)
