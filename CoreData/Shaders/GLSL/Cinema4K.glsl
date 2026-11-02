#include "Uniforms.glsl"
#include "Samplers.glsl"
#include "Transform.glsl"
#include "ScreenPos.glsl"

varying vec2 vTexCoord;
varying vec2 vScreenPos;

#ifdef COMPILEPS
uniform float cCinemaExposure;
uniform float cCinemaSaturation;
uniform float cChromaStrength;
uniform float cSharpenStrength;
uniform float cVignetteStrength;
uniform vec2 cLDR4KInvSize;

vec3 ACESFilm(vec3 x)
{
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}
#endif

void VS()
{
    mat4 modelMatrix = iModelMatrix;
    vec3 worldPos = GetWorldPos(modelMatrix);
    gl_Position = GetClipPos(worldPos);
    vTexCoord = GetQuadTexCoord(gl_Position);
    vScreenPos = GetScreenPosPreDiv(gl_Position);
}

void PS()
{
    #ifdef TONEMAP
    vec2 centered = vScreenPos - vec2(0.5, 0.5);
    float edgeDist2 = dot(centered, centered);

    // Subtle optical chromatic dispersion only near frame corners
    vec2 caOffset = centered * (edgeDist2 * cChromaStrength);
    float r = texture2D(sDiffMap, clamp(vScreenPos + caOffset, 0.0005, 0.9995)).r;
    float g = texture2D(sDiffMap, vScreenPos).g;
    float b = texture2D(sDiffMap, clamp(vScreenPos - caOffset, 0.0005, 0.9995)).b;
    vec3 hdr = max(vec3(r, g, b) * cCinemaExposure, vec3(0.0));

    // White-point normalized ACES Filmic Tonemap (prevents whiteout while keeping deep blacks)
    vec3 whiteScale = vec3(1.0) / max(ACESFilm(vec3(6.0)), vec3(0.001));
    vec3 col = clamp(ACESFilm(hdr) * whiteScale, 0.0, 1.0);

    // Subtle cyber-cinema color grading: rich saturation + crisp highlight warmth / cool shadow depth
    float luma = dot(col, vec3(0.2126, 0.7152, 0.0722));
    col = mix(vec3(luma), col, cCinemaSaturation);
    vec3 shadowTint = vec3(0.96, 0.99, 1.04);
    vec3 hiTint     = vec3(1.03, 1.00, 0.97);
    col *= mix(shadowTint, hiTint, smoothstep(0.15, 0.85, luma));

    gl_FragColor = vec4(clamp(col, 0.0, 1.0), 1.0);
    #endif

    #ifdef CAS4K
    vec2 px = cLDR4KInvSize;
    vec3 cC = texture2D(sDiffMap, vTexCoord).rgb;
    vec3 cN = texture2D(sDiffMap, clamp(vTexCoord + vec2(0.0, -px.y), 0.0005, 0.9995)).rgb;
    vec3 cS = texture2D(sDiffMap, clamp(vTexCoord + vec2(0.0,  px.y), 0.0005, 0.9995)).rgb;
    vec3 cW = texture2D(sDiffMap, clamp(vTexCoord + vec2(-px.x, 0.0), 0.0005, 0.9995)).rgb;
    vec3 cE = texture2D(sDiffMap, clamp(vTexCoord + vec2( px.x, 0.0), 0.0005, 0.9995)).rgb;

    // FidelityFX-inspired Contrast-Adaptive Sharpening (CAS) for 4K micro-detail
    vec3 mnRGB = min(min(min(cN, cS), min(cW, cE)), cC);
    vec3 mxRGB = max(max(max(cN, cS), max(cW, cE)), cC);
    vec3 ampRGB = sqrt(clamp(min(mnRGB, vec3(1.0) - mxRGB) / max(mxRGB, vec3(1e-4)), 0.0, 1.0));
    float peak = -1.0 / mix(8.0, 4.8, clamp(cSharpenStrength, 0.0, 1.0));
    vec3 wRGB = ampRGB * peak;
    vec3 rcpWeight = vec3(1.0) / (vec3(1.0) + 4.0 * wRGB);
    vec3 sharp = clamp(((cN + cS + cW + cE) * wRGB + cC) * rcpWeight, 0.0, 1.0);

    // Subtle optical vignette
    vec2 d = (vTexCoord - vec2(0.5, 0.5)) * vec2(1.15, 1.0);
    float vig = clamp(1.0 - dot(d, d) * cVignetteStrength, 0.45, 1.0);
    gl_FragColor = vec4(sharp * vig, 1.0);
    #endif
}
