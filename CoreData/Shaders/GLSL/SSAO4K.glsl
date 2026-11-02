#include "Uniforms.glsl"
#include "Samplers.glsl"
#include "Transform.glsl"
#include "ScreenPos.glsl"

varying vec2 vTexCoord;
varying vec2 vScreenPos;

#ifdef COMPILEPS
uniform float cSSAOStrength;
uniform float cSSAORadius;
uniform vec2 cSSAOBufInvSize;
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
#if !defined(GL_ES) || __VERSION__ >= 300
    #ifdef AOCALC
    float dCenter = ReconstructDepth(texture2D(sDepthBuffer, vTexCoord).r);
    if (dCenter > 0.985)
    {
        gl_FragColor = vec4(1.0, 1.0, 1.0, 1.0);
        return;
    }

    vec2 px = cGBufferInvSize;
    float dR = ReconstructDepth(texture2D(sDepthBuffer, clamp(vTexCoord + vec2(px.x, 0.0), 0.001, 0.999)).r);
    float dL = ReconstructDepth(texture2D(sDepthBuffer, clamp(vTexCoord - vec2(px.x, 0.0), 0.001, 0.999)).r);
    float dU = ReconstructDepth(texture2D(sDepthBuffer, clamp(vTexCoord + vec2(0.0, px.y), 0.001, 0.999)).r);
    float dD = ReconstructDepth(texture2D(sDepthBuffer, clamp(vTexCoord - vec2(0.0, px.y), 0.001, 0.999)).r);

    // Slope reconstruction across silhouettes (avoids self-occlusion on flat floors/walls)
    float slopeX = (abs(dR - dCenter) < abs(dCenter - dL)) ? (dR - dCenter) : (dCenter - dL);
    float slopeY = (abs(dU - dCenter) < abs(dCenter - dD)) ? (dU - dCenter) : (dCenter - dD);
    slopeX /= max(px.x, 1e-6);
    slopeY /= max(px.y, 1e-6);

    float zMeters = max(dCenter * cFarClipPS, 0.8);
    float baseRadiusPx = clamp((cSSAORadius * 420.0) / zMeters, 2.5, 32.0);

    // Interleaved Gradient Noise rotation
    float ign = fract(52.9829189 * fract(dot(gl_FragCoord.xy, vec2(0.06711056, 0.00583715))));
    float baseAng = ign * 6.2831853;

    float occ = 0.0;
    for (int i = 0; i < 12; ++i)
    {
        float fi = (float(i) + 0.5) / 12.0;
        float ang = baseAng + float(i) * 2.3999632; // Golden angle
        vec2 dir = vec2(cos(ang), sin(ang));
        vec2 off = dir * (baseRadiusPx * (0.20 + 0.80 * fi)) * px;
        vec2 suv = clamp(vTexCoord + off, 0.001, 0.999);

        float dSample = ReconstructDepth(texture2D(sDepthBuffer, suv).r);
        float dExpected = dCenter + slopeX * (suv.x - vTexCoord.x) + slopeY * (suv.y - vTexCoord.y);
        float deltaM = (dExpected - dSample) * cFarClipPS;

        float rangeCheck = smoothstep(0.0, 1.0, 1.35 / (0.18 + abs(deltaM)));
        float hit = smoothstep(0.04, 0.24, deltaM);
        occ += hit * rangeCheck;
    }

    float ao = clamp(1.0 - (occ / 12.0) * 1.45, 0.15, 1.0);
    // Fade SSAO gracefully into distance fog
    ao = mix(ao, 1.0, smoothstep(0.35, 0.85, dCenter));
    gl_FragColor = vec4(ao, dCenter, 0.0, 1.0);
    #endif

    #ifdef AOAPPLY
    vec4 sceneCol = texture2D(sDiffMap, vScreenPos);
    float dCenter = ReconstructDepth(texture2D(sDepthBuffer, vTexCoord).r);
    if (dCenter > 0.985)
    {
        gl_FragColor = sceneCol;
        return;
    }

    // 5-tap depth-aware bilateral filter to eliminate half-res grain on sharp 4K edges
    vec2 stepUV = cSSAOBufInvSize;
    vec2 c0 = texture2D(sNormalMap, vTexCoord).rg;
    float aoSum = c0.r * 2.0;
    float wSum = 2.0;

    vec2 offs[4];
    offs[0] = vec2( stepUV.x, 0.0);
    offs[1] = vec2(-stepUV.x, 0.0);
    offs[2] = vec2(0.0,  stepUV.y);
    offs[3] = vec2(0.0, -stepUV.y);

    for (int k = 0; k < 4; ++k)
    {
        vec2 s = texture2D(sNormalMap, clamp(vTexCoord + offs[k], 0.001, 0.999)).rg;
        float w = exp(-abs(s.g - dCenter) * 450.0);
        aoSum += s.r * w;
        wSum += w;
    }

    float ao = aoSum / max(wSum, 1e-4);
    // Preserve bright HDR emissives/highlights so neon & lasers keep full punch
    float lum = dot(sceneCol.rgb, vec3(0.2126, 0.7152, 0.0722));
    float strength = cSSAOStrength * (1.0 - smoothstep(0.95, 2.2, lum) * 0.75);
    vec3 finalRgb = sceneCol.rgb * mix(1.0, ao, clamp(strength, 0.0, 1.0));
    gl_FragColor = vec4(finalRgb, sceneCol.a);
    #endif
#else
    gl_FragColor = texture2D(sDiffMap, vScreenPos);
#endif
}
