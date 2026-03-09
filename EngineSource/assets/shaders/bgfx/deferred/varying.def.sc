vec3 v_worldPos   : TEXCOORD0 = vec3(0.0, 0.0, 0.0);
vec3 v_worldNormal: TEXCOORD1 = vec3(0.0, 1.0, 0.0);
vec2 v_texcoord0  : TEXCOORD2 = vec2(0.0, 0.0);
vec2 v_velocity   : TEXCOORD3 = vec2(0.0, 0.0);
vec3 v_worldTangent: TEXCOORD4 = vec3(1.0, 0.0, 0.0);

vec3 a_position   : POSITION;
vec3 a_normal     : NORMAL;
vec2 a_texcoord0  : TEXCOORD0;
vec2 a_texcoord1  : TEXCOORD1;
vec3 a_tangent    : TANGENT;
vec4 a_indices    : BLENDINDICES;
vec4 a_weight     : BLENDWEIGHT;
