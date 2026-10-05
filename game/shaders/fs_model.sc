$input v_texcoord0, v_worldNormal, v_worldPosition

#include "common.sh"
#include "sun_shadows.sh"

SAMPLER2D(s_modelTexture, 0);
SAMPLER2D(s_modelNormal, 1);
SAMPLER2D(s_modelMetallicRoughness, 2);
SAMPLERCUBE(s_modelEnvironment, 3);
SAMPLER2D(s_modelEnvironmentBrdf, 4);
uniform vec4 u_modelEnvironment;
uniform vec4 u_modelSurface;
uniform vec4 u_modelPbr;
uniform vec4 u_modelMaterial[2];
uniform vec4 u_modelLighting[4];
uniform vec4 u_modelPointPositions[12];
uniform vec4 u_modelPointColors[12];
uniform vec4 u_modelFog[3];
uniform vec4 u_modelCamera;
uniform vec4 u_modelOutline;

vec3 modelNormalize(vec3 value)
{
    return value * inversesqrt(max(dot(value, value), 0.000001));
}

vec3 modelEncodeSrgb(vec3 value)
{
    value = max(value, vec3_splat(0.0));
    return mix(value * 12.92, 1.055 * pow(value, vec3_splat(1.0 / 2.4)) - 0.055,
        step(vec3_splat(0.0031308), value));
}

vec3 modelBrdf(vec3 normal, vec3 view, vec3 light, vec3 base, float metallic, float roughness)
{
    float nl = max(dot(normal, light), 0.0);
    float nv = max(dot(normal, view), 0.0001);
    vec3 halfDirection = modelNormalize(light + view);
    float nh = max(dot(normal, halfDirection), 0.0);
    float vh = max(dot(view, halfDirection), 0.0);
    float alpha = roughness * roughness;
    float alphaSquared = alpha * alpha;
    float denominator = nh * nh * (alphaSquared - 1.0) + 1.0;
    float distribution = alphaSquared / max(3.14159265 * denominator * denominator, 0.0000001);
    float k = (roughness + 1.0) * (roughness + 1.0) / 8.0;
    float geometry = (nl / max(nl * (1.0 - k) + k, 0.0001))
        * (nv / max(nv * (1.0 - k) + k, 0.0001));
    vec3 f0 = mix(vec3_splat(0.04), base, metallic);
    vec3 fresnel = f0 + (1.0 - f0) * pow(1.0 - vh, 5.0);
    vec3 diffuse = (1.0 - fresnel) * (1.0 - metallic) * base / 3.14159265;
    vec3 specular = distribution * geometry * fresnel / max(4.0 * nl * nv, 0.0001);
    // Existing light intensities use unit diffuse irradiance, rather than radiance divided by pi.
    return (diffuse + specular) * (nl * 3.14159265);
}

float modelSmoothstep(float low, float high, float value)
{
    float amount = clamp((value - low) / max(high - low, 1.0), 0.0, 1.0);
    return amount * amount * (3.0 - 2.0 * amount);
}

void main()
{
    // Base textures are sRGB; the sampler decodes RGB, while factors and alpha stay linear.
    vec4 color = texture2D(s_modelTexture, v_texcoord0) * u_modelMaterial[0];
    if (u_modelMaterial[1].y > 0.5 && color.a < u_modelMaterial[1].x)
    {
        discard;
    }
    if (u_modelOutline.w > 0.5)
    {
        gl_FragColor = vec4(u_modelOutline.rgb, 1.0);
        return;
    }
    vec3 shaded = color.rgb;
    if (u_modelMaterial[1].z < 0.5)
    {
        vec3 normal = modelNormalize(v_worldNormal);
        if (u_modelSurface.w >= 0.0)
        {
            vec3 dp1 = dFdx(v_worldPosition);
            vec3 dp2 = dFdy(v_worldPosition);
            vec2 duv1 = dFdx(v_texcoord0);
            vec2 duv2 = dFdy(v_texcoord0);
            vec3 p1 = cross(normal, dp1);
            vec3 p2 = cross(dp2, normal);
            vec3 tangent = p2 * duv1.x + p1 * duv2.x;
            vec3 bitangent = p2 * duv1.y + p1 * duv2.y;
            float inverseLength = inversesqrt(max(max(dot(tangent, tangent), dot(bitangent, bitangent)), 0.000001));
            vec3 mapped = texture2D(s_modelNormal, v_texcoord0).xyz * 2.0 - 1.0;
            mapped.xy *= u_modelSurface.w;
            normal = modelNormalize(tangent * inverseLength * mapped.x
                + bitangent * inverseLength * mapped.y + normal * mapped.z);
        }
        vec3 view = modelNormalize(u_modelCamera.xyz - v_worldPosition);
        if (u_modelMaterial[1].w > 0.5 && dot(normal, view) < 0.0)
        {
            normal = -normal;
        }
        vec4 surface = texture2D(s_modelMetallicRoughness, v_texcoord0);
        float metallic = clamp(surface.b * u_modelPbr.x, 0.0, 1.0);
        float roughness = clamp(surface.g * u_modelPbr.y, 0.045, 1.0);
        float nv = max(dot(normal, view), 0.0001);
        vec3 f0 = mix(vec3_splat(0.04), color.rgb, metallic);
        vec3 fresnel = f0 + (max(vec3_splat(1.0 - roughness), f0) - f0) * pow(1.0 - nv, 5.0);
        // ponytail: upward probe fill uses a hemisphere; directional indirect probes if this loses needed contrast.
        shaded = color.rgb * (1.0 - metallic) * (1.0 - fresnel) * u_modelLighting[2].rgb * u_modelLighting[2].w
            * (0.65 + 0.35 * max(normal.z, 0.0));
        vec3 reflected = textureCubeLod(s_modelEnvironment, reflect(-view, normal),
            roughness * u_modelEnvironment.w).rgb;
        vec2 integrated = texture2D(s_modelEnvironmentBrdf, vec2(nv, roughness)).rg;
        shaded += reflected * u_modelEnvironment.rgb * (f0 * integrated.x + integrated.y);
        shaded += modelBrdf(normal, view, modelNormalize(u_modelLighting[0].xyz), color.rgb, metallic, roughness)
            * u_modelLighting[1].rgb * u_modelLighting[0].w
            * sunShadowVisibility(v_worldPosition, modelNormalize(v_worldNormal));
        for (int i = 0; i < 12; ++i)
        {
            if (i >= int(u_modelLighting[3].w))
            {
                break;
            }
            vec3 delta = u_modelPointPositions[i].xyz - v_worldPosition;
            float radius = max(u_modelPointPositions[i].w, 0.0001);
            float attenuation = max(1.0 - dot(delta, delta) / (radius * radius), 0.0);
            shaded += modelBrdf(normal, view, modelNormalize(delta), color.rgb, metallic, roughness)
                * u_modelPointColors[i].rgb * u_modelPointColors[i].w * attenuation * attenuation;
        }
    }
    shaded += u_modelSurface.xyz;
    // An SDR shoulder preserves sub-0.8 values and hue while compressing specular peaks smoothly.
    float peak = max(max(shaded.r, shaded.g), shaded.b);
    float compressed = 0.8 + 0.2 * (1.0 - exp(-max(peak - 0.8, 0.0) / 0.2));
    if (u_modelMaterial[1].z < 0.5 && peak > 0.8)
    {
        shaded *= compressed / peak;
    }
    float distance = length(v_worldPosition - u_modelCamera.xyz);
    float fog = u_modelFog[1].w > 0.5
        ? clamp((distance - u_modelFog[2].x) / max(u_modelFog[2].y - u_modelFog[2].x, 1.0), 0.0, 1.0)
        : u_modelFog[1].x
            + (u_modelFog[1].y - u_modelFog[1].x) * modelSmoothstep(u_modelFog[2].x, u_modelFog[2].y, distance)
            + (1.0 - u_modelFog[1].y) * modelSmoothstep(u_modelFog[2].y, u_modelFog[2].z, distance);
    fog = max(fog, u_modelFog[1].z);
    gl_FragColor = vec4(mix(modelEncodeSrgb(shaded), u_modelFog[0].rgb, fog), color.a);
}
