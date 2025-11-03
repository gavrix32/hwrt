slangc slang/raytrace.rgen.slang -target spirv -profile spirv_1_4 -matrix-layout-column-major -o spirv/raytrace.rgen.spv
slangc slang/raytrace.rmiss.slang -target spirv -profile spirv_1_4 -matrix-layout-column-major -o spirv/raytrace.rmiss.spv
slangc slang/raytrace.rchit.slang -target spirv -profile spirv_1_4 -matrix-layout-column-major -o spirv/raytrace.rchit.spv
