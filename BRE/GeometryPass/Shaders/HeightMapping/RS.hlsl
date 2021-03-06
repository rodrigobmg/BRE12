#define RS \
"RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT | " \
"DENY_HULL_SHADER_ROOT_ACCESS | " \
"DENY_GEOMETRY_SHADER_ROOT_ACCESS), " \
"DescriptorTable(CBV(b0), visibility = SHADER_VISIBILITY_VERTEX), " \
"CBV(b1, visibility = SHADER_VISIBILITY_VERTEX), " \
"CBV(b0, visibility = SHADER_VISIBILITY_DOMAIN), " \
"DescriptorTable(SRV(t0), visibility = SHADER_VISIBILITY_DOMAIN), " \
"DescriptorTable(CBV(b0), visibility = SHADER_VISIBILITY_PIXEL), " \
"CBV(b1, visibility = SHADER_VISIBILITY_PIXEL), " \
"DescriptorTable(SRV(t0), visibility = SHADER_VISIBILITY_PIXEL), " \
"DescriptorTable(SRV(t1), visibility = SHADER_VISIBILITY_PIXEL), " \
"StaticSampler(s0, filter=FILTER_ANISOTROPIC)"