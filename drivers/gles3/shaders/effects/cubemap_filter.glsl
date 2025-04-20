/* clang-format off */
#[modes]

mode_default =
mode_copy = #define MODE_DIRECT_WRITE
mode_sh = #define MODE_SPHERICAL_HARMONICS

#[specializations]

#[vertex]

layout(location = 0) in highp vec2 vertex_attrib;
/* clang-format on */

out highp vec2 uv_interp;

void main() {
	uv_interp = vertex_attrib;
	gl_Position = vec4(uv_interp, 0.0, 1.0);
}

/* clang-format off */
#[fragment]


#define M_PI 3.14159265359

uniform samplerCube source_cube; //texunit:0

/* clang-format on */

#ifndef MODE_SPHERICAL_HARMONICS

uniform int face_id;

#ifndef MODE_DIRECT_WRITE
uniform uint sample_count;
uniform vec4 sample_directions_mip[MAX_SAMPLE_COUNT];
uniform float weight;
#endif

#endif // !MODE_SPHERICAL_HARMONICS

in highp vec2 uv_interp;

#ifndef MODE_SPHERICAL_HARMONICS
layout(location = 0) out vec4 frag_color;
#else // !MODE_SPHERICAL_HARMONICS
layout(location = 0) out vec4 sh_l0;
layout(location = 1) out vec4 sh_l1n;
layout(location = 2) out vec4 sh_l10;
layout(location = 3) out vec4 sh_l1p;
#endif // MODE_SPHERICAL_HARMONICS

#define M_PI 3.14159265359

// Don't include tonemap_inc.glsl because all we want is these functions, we don't want the uniforms
vec3 linear_to_srgb(vec3 color) {
	return max(vec3(1.055) * pow(color, vec3(0.416666667)) - vec3(0.055), vec3(0.0));
}

vec3 srgb_to_linear(vec3 color) {
	return color * (color * (color * 0.305306011 + 0.682171111) + 0.012522878);
}

vec3 texelCoordToVec(vec2 uv, int faceID) {
	mat3 faceUvVectors[6];

	// -x
	faceUvVectors[1][0] = vec3(0.0, 0.0, 1.0); // u -> +z
	faceUvVectors[1][1] = vec3(0.0, -1.0, 0.0); // v -> -y
	faceUvVectors[1][2] = vec3(-1.0, 0.0, 0.0); // -x face

	// +x
	faceUvVectors[0][0] = vec3(0.0, 0.0, -1.0); // u -> -z
	faceUvVectors[0][1] = vec3(0.0, -1.0, 0.0); // v -> -y
	faceUvVectors[0][2] = vec3(1.0, 0.0, 0.0); // +x face

	// -y
	faceUvVectors[3][0] = vec3(1.0, 0.0, 0.0); // u -> +x
	faceUvVectors[3][1] = vec3(0.0, 0.0, -1.0); // v -> -z
	faceUvVectors[3][2] = vec3(0.0, -1.0, 0.0); // -y face

	// +y
	faceUvVectors[2][0] = vec3(1.0, 0.0, 0.0); // u -> +x
	faceUvVectors[2][1] = vec3(0.0, 0.0, 1.0); // v -> +z
	faceUvVectors[2][2] = vec3(0.0, 1.0, 0.0); // +y face

	// -z
	faceUvVectors[5][0] = vec3(-1.0, 0.0, 0.0); // u -> -x
	faceUvVectors[5][1] = vec3(0.0, -1.0, 0.0); // v -> -y
	faceUvVectors[5][2] = vec3(0.0, 0.0, -1.0); // -z face

	// +z
	faceUvVectors[4][0] = vec3(1.0, 0.0, 0.0); // u -> +x
	faceUvVectors[4][1] = vec3(0.0, -1.0, 0.0); // v -> -y
	faceUvVectors[4][2] = vec3(0.0, 0.0, 1.0); // +z face

	// out = u * s_faceUv[0] + v * s_faceUv[1] + s_faceUv[2].
	vec3 result = (faceUvVectors[faceID][0] * uv.x) + (faceUvVectors[faceID][1] * uv.y) + faceUvVectors[faceID][2];
	return normalize(result);
}

void main() {
#ifndef MODE_SPHERICAL_HARMONICS
	vec3 color = vec3(0.0);
	vec2 uv = uv_interp;
	vec3 N = texelCoordToVec(uv, face_id);

#ifdef MODE_DIRECT_WRITE
	frag_color = vec4(textureLod(source_cube, N, 0.0).rgb, 1.0);
#else

	vec4 sum = vec4(0.0);
	vec3 UpVector = abs(N.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
	mat3 T;
	T[0] = normalize(cross(UpVector, N));
	T[1] = cross(N, T[0]);
	T[2] = N;

	for (uint sample_num = 0u; sample_num < sample_count; sample_num++) {
		vec4 sample_direction_mip = sample_directions_mip[sample_num];
		vec3 L = T * sample_direction_mip.xyz;
		vec3 val = textureLod(source_cube, L, sample_direction_mip.w).rgb;
		// Mix using linear
		val = srgb_to_linear(val);
		sum.rgb += val * sample_direction_mip.z;
	}

	sum /= weight;

	sum.rgb = linear_to_srgb(sum.rgb);
	frag_color = vec4(sum.rgb, 1.0);
#endif

#else // !MODE_SPHERICAL_HARMONICS
	float theta = 0.0;
	float phi = 0.0;

	vec4 resl0 = vec4(0.0);
	vec4 resl1n = vec4(0.0);
	vec4 resl10 = vec4(0.0);
	vec4 resl1p = vec4(0.0);

	for (uint i = 0u; i < 32u; i++) {
		for (uint j = 0u; j < 32u; j++) {
			vec3 N = vec3(sin(theta) * cos(phi), sin(theta) * sin(phi), cos(theta));
			vec4 col = textureLod(source_cube, N, 0.0);

			resl0 += col * 0.28209479177387814;
			resl1n += col * 0.4886025119029199 * N.y;
			resl10 += col * 0.4886025119029199 * N.z;
			resl1p += col * 0.4886025119029199 * N.x;

			phi += M_PI / 32;
		}
		theta += M_PI / 32;
	}

	sh_l0 = resl0 / (32 * 32);
	sh_l1n = resl1n / (32 * 32);
	sh_l10 = resl10 / (32 * 32);
	sh_l1p = resl1p / (32 * 32);
#endif // MODE_SPHERICAL_HARMONICS
}
