// #version 330 core

out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D scene;
uniform sampler2D bloomBlur;
uniform float exposure;
uniform float bloomStrength = 0.04f;
uniform int programChoice;

#if defined(GL_ES)
  #define HIGHP highp
#else
  #define HIGHP
#endif

const HIGHP float NOISE_GRANULARITY = 0.5/255.0;

HIGHP float random(vec2 uv) {
    return fract(sin(dot(uv, vec2(12.9898,78.233))) * 43758.5453);
}

vec3 bloom_none()
{
    vec3 hdrColor = texture(scene, TexCoords).rgb;
    return hdrColor;
}

vec3 bloom_old()
{
    vec3 hdrColor = texture(scene, TexCoords).rgb;
    vec3 bloomColor = texture(bloomBlur, TexCoords).rgb;
    return hdrColor + bloomColor; // additive blending
}

vec3 bloom_new()
{
    vec3 hdrColor = texture(scene, TexCoords).rgb;
    vec3 bloomColor = texture(bloomBlur, TexCoords).rgb;
    return mix(hdrColor, bloomColor, bloomStrength); // linear interpolation
}

vec3 bloom_blur()
{
    vec3 bloomColor = texture(bloomBlur, TexCoords).rgb;
    return bloomColor;
}

void main()
{
    // to bloom or not to bloom
    vec3 result = vec3(0.0);
    switch (programChoice)
    {
    case 1: result = bloom_none(); break;
    case 2: result = bloom_old(); break;
    case 3: result = bloom_new(); break;
    case 4: result = bloom_blur(); break;
    default:
        result = bloom_none(); break;
    }

    // Add dithering
    result += mix(-NOISE_GRANULARITY, NOISE_GRANULARITY, random(gl_FragCoord.xy));

    // tone mapping: so far I have not been able to use
    // this correctly due to my main texture not being HDR
    // result = vec3(1.0) - exp(-result * exposure);
    // also gamma correct while we're at it
    // const float gamma = 2.2;
    // result = pow(result, vec3(1.0 / gamma));
    FragColor = vec4(result, 1.0);
}

