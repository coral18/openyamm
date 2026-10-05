$input v_texcoord0

#include "common.sh"

SAMPLER2D(s_modelTexture, 0);
uniform vec4 u_modelMaterial[2];

void main()
{
    if (u_modelMaterial[1].y > 0.5
        && texture2D(s_modelTexture, v_texcoord0).a * u_modelMaterial[0].a < u_modelMaterial[1].x)
    {
        discard;
    }
    // Hardware depth selects the nearest caster; ordinary colour channels retain 24-bit sampling precision.
    vec3 encoded = fract(min(gl_FragCoord.z, 0.9999999) * vec3(1.0, 255.0, 65025.0));
    encoded.xy -= encoded.yz / 255.0;
    gl_FragColor = vec4(encoded, 1.0);
}
