$input v_texcoord0, v_worldNormal

#include "common.sh"

SAMPLER2D(s_modelTexture, 0);
uniform vec4 u_modelMaterial[2];
uniform vec4 u_modelLighting[2];

void main()
{
    vec4 color = texture2D(s_modelTexture, v_texcoord0) * u_modelMaterial[0];
    if (u_modelMaterial[1].y > 0.5 && color.a < u_modelMaterial[1].x)
    {
        discard;
    }

    vec3 normal = normalize(v_worldNormal);
    vec3 lightDirection = normalize(u_modelLighting[0].xyz);
    float diffuse = max(dot(normal, lightDirection), 0.0);
    float lit = min(u_modelLighting[0].w + diffuse * u_modelLighting[1].x, 1.0);
    float lighting = mix(lit, 1.0, u_modelMaterial[1].z);
    gl_FragColor = vec4(color.rgb * lighting, color.a);
}
