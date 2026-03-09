$input v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2D(u_SceneTexture, 0);
SAMPLER2D(u_BloomTexture, 1);

uniform vec4 u_BloomCompositeParams; // x = bloom intensity

void main()
{
	vec3 scene = texture2D(u_SceneTexture, v_texcoord0).rgb;
	vec3 bloom = texture2D(u_BloomTexture, v_texcoord0).rgb;
	vec3 result = scene + bloom * u_BloomCompositeParams.x;
	gl_FragColor = vec4(result, 1.0);
}
