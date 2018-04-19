const int MATERIAL_COUNT = 64;

const uint MATFLAG_DIFFUSEMAP = 0x0001;
const uint MATFLAG_BUMPMAP = 0x0002;
const uint MATFLAG_SPECMAP = 0x0004;
const uint MATFLAG_NORMALMAP = 0x0008;
const uint MATFLAG_PRELIT = 0x0010;
const uint MATFLAG_ALPHAMASK = 0x0020;

const uint TEXLAYER_DIFFUSE = 0;
const uint TEXLAYER_BUMP = 1;
const uint TEXLAYER_SPEC = 2;
const uint TEXLAYER_ALPHA = 3;

const uint SCENEFLAG_ENABLESHADOWS = 0x0001;
const uint SCENEFLAG_PRELIT = 0x0002;
const uint SCENEFLAG_ENABLEBUMPMAPS = 0x0004;
const uint SCENEFLAG_MAPSPLIT = 0x0008;
const uint SCENEFLAG_SHOWNORMALS = 0x0010;
const uint SCENEFLAG_ENABLESPECMAPS = 0x0020;
const uint SCENEFLAG_ENABLEPCF = 0x0040;

const float bumpMapIntensity = 1.0;
const float SHADOW_BIAS = 0.0005;
const float SHADOW_MUL = 0.3;

const mat4 biasMatrix = mat4( 
	0.5, 0.0, 0.0, 0.0,
	0.0, 0.5, 0.0, 0.0,
	0.0, 0.0, 1.0, 0.0,
	0.5, 0.5, 0.0, 1.0 
);

struct LightData {
	mat4 mvp;
	vec4 color;
	vec3 pos;
};

struct MaterialData {
    vec4 ambient[MATERIAL_COUNT];
    vec4 diffuse[MATERIAL_COUNT];
    vec4 specular[MATERIAL_COUNT];
    vec4 emissive[MATERIAL_COUNT];
    vec4 transparency[MATERIAL_COUNT];
    uint flags[MATERIAL_COUNT];
    float shininess[MATERIAL_COUNT];
};

struct Model {
    mat4 pos;
    float scale;
};

struct Camera {
    mat4 projview;
	mat4 invProj;
    vec4 pos;
	uint width;
	uint height;
};

bool flag(uint set, uint mask)
{
    return (set & mask) == mask;
}

