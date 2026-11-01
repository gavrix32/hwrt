#!/bin/bash

SHADER_DIR="$(cd "$(dirname "$0")" && pwd)/slang"
OUTPUT_DIR="$SHADER_DIR/../spirv"

mkdir -p "$OUTPUT_DIR"

echo "Compiling shaders..."

# Shaders include shared headers (common.h, atmosphere.slang, ...), so a shader must be rebuilt
# whenever any source in the directory is newer than its SPIR-V
NEWEST_SOURCE=$(ls -t "$SHADER_DIR"/*.slang "$SHADER_DIR"/*.h 2>/dev/null | head -n 1)

for SHADER in raytrace.rgen raytrace.rmiss raytrace.rchit raytrace.rahit compute; do
    SRC="$SHADER_DIR/$SHADER.slang"
    DST="$OUTPUT_DIR/$SHADER.spv"

    if [[ ! -f "$DST" || "$SRC" -nt "$DST" || "$NEWEST_SOURCE" -nt "$DST" ]]; then
        echo "- $SHADER"
        slangc -I "$SHADER_DIR" "$SRC" -target spirv -profile spirv_1_6 -matrix-layout-column-major -fvk-use-scalar-layout -capability spvShaderClockKHR -o "$DST"

        if [[ $? -ne 0 ]]; then
            echo "Failed to compile $SHADER"
            exit 1
        fi
    fi
done

echo "Done"