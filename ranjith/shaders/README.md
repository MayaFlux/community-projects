# Curated Shaders

A handful of curated MayaFlux compute and fragment shaders for reuse in other projects. These are shader assets, not complete applications. Compile each source to a matching `.spv` file before loading it.

## Fragment shader bindings

All eight `.frag` files take UV coordinates at location 0, write a color at location 0, and sample a `sampler2DArray` at set 0, binding 1. Supply up to 30 populated array layers. Their push constants have the same layout: `uint layer_count`, `uint mode`, then `float weights[30]` (128 bytes total). Set `layer_count` to the number of populated layers, even where a shader ignores `mode` or `weights`. Shaders using `LayerData` additionally require a read-only `vec4` array at set 1, binding 0. Use a compatible textured quad vertex shader, such as MayaFlux's shipped `texture.vert`, and match the array's layer order to the descriptions below.

With `TextureArrayBuffer`, the push block is supplied automatically and weights initially equal 1. Chimera passes each layer's level into its weight slot. The full block remains in every shader for compatibility with those callers. If binding a custom renderer, push all 128 bytes explicitly; Vulkan does not initialize omitted push constant bytes. Zero weights suppress layers or differences in the shaders that read them.

### Per-pixel delay

`texture_array_delay.frag` reads a red-channel delay map at set 0, binding 2. Map value 0 selects layer 0, value 1 selects the last populated layer, and intermediate values interpolate. Set `mode` to 1 to reverse the map, or 0 to use it directly; weights are unused. Supply at least one layer.

### Temporal order statistic

`texture_array_order.frag` sorts the value of each RGBA channel independently at every pixel. Set `mode` to 0 for median, 1 for minimum, or 2 for maximum; only layers whose weight is positive participate. A channel's result may come from a different frame than the other channels. With no participating layers, the shader outputs transparent black. Cost grows with the number of layers.

### Temporal difference

`texture_array_diff.frag` adds absolute RGB differences between neighboring layers. Set `mode` to 0 for color differences, 1 for luminance, or 2 for the signed difference of layers 0 and 1 centered on gray. `weights[i]` scales the difference of layers `i` and `i + 1`; modes 0 and 1 skip nonpositive weights. Supply at least two layers to obtain a difference.

### Paint with time

`paint_time.frag` treats layer 0 as the live image and uses its brightness to choose between recorded layers 1 onward. Put the depth control in `LayerData[0].x`, between 0 and 1; mode and weights are unused. Supply at least three layers with the current shader.

### Emergence

`emerge.frag` sorts each pixel's RGB values across layers, then picks a spatially scattered quantile. Set `LayerData[0].x` to the base quantile, `.y` to scatter strength, and `.z` to the changing seed; mode and weights are unused. Supply at least one layer. Its per-pixel sort is relatively expensive with many layers.

### Uncanny

`uncanny.frag` uses moving noise to select a layer at each pixel and darkens the seams between selections. Put the motion phase in `LayerData[0].x`; give each layer two `vec4` entries, with `LayerData[2 * i + 1].w` at least 0.5 for a valid layer. Mode and weights are unused. Supply at least one layer and a metadata entry for every selected layer.

### Echo

`echo.frag` starts with the live image in layer 0, then lets recorded layers claim regions through a noise field bent by live brightness. Give each layer a params/timing pair of `vec4` entries: params `.xy` offset the field and `.z` sets its scale; timing `.x` is age in seconds and `.w` marks a valid recording. `weights[i]` controls the claimed area for layer `i`; mode is unused. Supply at least one layer. The field uses a fixed 16:9 aspect ratio.

### Placed parts

`texture_array_parts.frag` places and composites array layers in order. For each layer, put its center in params `.xy` (UV space), scale in `.z`, and rotation in `.w` (radians) at `LayerData[2 * i]`; the following timing entry is reserved. `weights[i]` controls its opacity, so use values from 0 to 1; mode is unused. Supply a metadata pair for each populated layer.

## Compute shaders

### Tile recompose

`tile_recompose.comp` rearranges tiles of a flat float image using a tile-index map. Bind source pixels at set 0, binding 0; tile indices at binding 1; and an output pixel buffer at binding 2. Supply image size, tile size, and channel count as push constants; use dimensions divisible by the tile size and valid tile indices.

### Proximity graph

`graph_build.comp` connects points within a radius when their attributes are sufficiently close. Bind packed 2D or 3D positions, per-point attributes, an edge-pair output buffer, and a zeroed edge counter at set 0, bindings 0 through 3. Supply point count, coordinate stride, radius, attribute threshold, and edge capacity as push constants; work grows quadratically with point count.

### Instance wave

`wave_field.comp` animates the Z translation of a grid of instance transforms with a diagonal sine wave. Bind a writable buffer of initialized, column-major matrices (16 floats per instance) at set 0, binding 0. Supply slot count, nonzero grid width, time, amplitude, frequency, and speed as push constants.
