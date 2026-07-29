#version 440

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    float uTime;
    float uLevel;
    float uPulse;
    vec2 uResolution;
    vec3 uColorLight;
    vec3 uColorMid;
    vec3 uColorDeep;

    float uFlowSpeed;      // 流动速度        建议 0.1 ~ 1.5   默认 0.4
    float uScale;          // 分块大小(缩放)  建议 0.6 ~ 2.5   默认 1.2（越大斑块越多越小）
    float uWarpStrength;   // 弯曲/扭曲程度   建议 0.15 ~ 0.8  默认 0.35（不要超过1，否则又会出现细丝）
    float uDetail;         // 细节精细程度    建议 2.0 ~ 5.0   默认 4.0
    float uVorticity;      // 涡流强度        建议 0.0 ~ 0.6   默认 0.15
    float uNormalStrength; // 法线明暗强度    建议 0.0 ~ 0.15  默认 0.05（只做轻微质感，别调太大）
    float uCloudContrast;  // 云团对比度      建议 1.0 ~ 4.0   默认 2.2（越大云团边界越清晰、留白越多）
    float uCloudCoverage;  // 云团覆盖率      建议 0.3 ~ 0.7   默认 0.48（阈值，越小云越多，越大留白越多）
    float uSpeedSoftness;

    vec3  uColorFast;        // 高速流动区域的颜色（建议青色，如 0.0,0.95,0.9）
    float uSpeedColorAmount; // 流速->颜色 的影响强度  建议 0.0~2.0  默认 0.8
    float uShadingStrength;  // 明暗对比强度(独立于流速颜色) 建议 0.0~1.0 默认 0.35

};

const int MAX_OCTAVES = 6;
const mat2 OCTAVE_ROT = mat2(0.8, -0.6, 0.6, 0.8);

float hash(vec2 p) {
    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

float noise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    vec2 u = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash(i), hash(i + vec2(1.0, 0.0)), u.x),
               mix(hash(i + vec2(0.0, 1.0)), hash(i + vec2(1.0, 1.0)), u.x), u.y);
}


float fbm(vec2 p, float detail) {
    float value = 0.0;
    float amplitude = 0.5;
    int oct = int(clamp(detail, 1.0, float(MAX_OCTAVES)));
    for (int i = 0; i < MAX_OCTAVES; i++) {
        if (i >= oct) break;
        value += amplitude * noise(p);
        p = OCTAVE_ROT * p * 1.85 + vec2(37.2, 17.9); 
        amplitude *= 0.5;
    }
    return value;
}

float fluidPattern(vec2 p, float t, float scale, float warp, float detail, float vorticity) {
    p *= scale;

    if (vorticity > 0.0005) {
        float angle = (noise(p * 0.35 + t * 0.08) - 0.5) * vorticity * 3.14159;
        float s = sin(angle), c = cos(angle);
        p = mat2(c, -s, s, c) * p;
    }

    vec2 q = vec2(
        fbm(p + vec2(1.7, 4.3) + t * 0.06, detail),
        fbm(p + vec2(9.2, 2.8) - t * 0.05, detail)
    );

    float n = fbm(p + warp * q + vec2(3.3, -6.1), detail);
    return n;
}

// 把原始fbm值整理成"团块状"云雾：对比度拉伸 + 覆盖率阈值
// 这样才会呈现"有的地方浓、有的地方完全没有"的云彩感，而不是连续的细纹理
float toCloudMask(float n, float contrast, float coverage) {
    float d = n - coverage;
    d *= contrast;
    return clamp(d + 0.5, 0.0, 1.0);
}

float getRawDensity(vec2 uv, float t, float speed, float scale, float warp, float detail, float vorticity) {
    vec2 flowUv = uv * 1.3;
    flowUv.x += t * speed * 0.5;
    flowUv.y += sin(t * speed * 0.35) * 0.10;
    return fluidPattern(flowUv, t * speed, scale, warp, detail, vorticity);
}

vec2 getGrad(vec2 uv, float delta, float t, float speed, float scale, float warp, float detail, float vorticity) {
    float dx = getRawDensity(uv + vec2(delta, 0.0), t, speed, scale, warp, detail, vorticity)
             - getRawDensity(uv - vec2(delta, 0.0), t, speed, scale, warp, detail, vorticity);
    float dy = getRawDensity(uv + vec2(0.0, delta), t, speed, scale, warp, detail, vorticity)
             - getRawDensity(uv - vec2(0.0, delta), t, speed, scale, warp, detail, vorticity);
    return vec2(dx, dy) / (2.0 * delta);
}

float getSmoothFlowSpeed(vec2 uv, float t, float speed, float scale, float warp, float detail, float vorticity, float bigDelta) {
    vec2 g0 = getGrad(uv, bigDelta, t, speed, scale, warp, detail, vorticity);
    vec2 g1 = getGrad(uv + vec2(bigDelta * 0.5, -bigDelta * 0.5), bigDelta, t, speed, scale, warp, detail, vorticity);
    vec2 g2 = getGrad(uv - vec2(bigDelta * 0.5, -bigDelta * 0.5), bigDelta, t, speed, scale, warp, detail, vorticity);
    float m = (length(g0) + length(g1) + length(g2)) / 3.0;
    return m;
}

float getCoarseDensity(vec2 uv, float t, float speed, float scale, float warp, float vorticity) {
    vec2 flowUv = uv * 1.3;
    flowUv.x += t * speed * 0.5;
    flowUv.y += sin(t * speed * 0.35) * 0.10;
    return fluidPattern(flowUv, t * speed, scale, warp, 2.0, vorticity); // detail写死为2.0
}

vec2 getCoarseGrad(vec2 uv, float delta, float t, float speed, float scale, float warp, float vorticity) {
    float dx = getCoarseDensity(uv + vec2(delta, 0.0), t, speed, scale, warp, vorticity)
             - getCoarseDensity(uv - vec2(delta, 0.0), t, speed, scale, warp, vorticity);
    float dy = getCoarseDensity(uv + vec2(0.0, delta), t, speed, scale, warp, vorticity)
             - getCoarseDensity(uv - vec2(0.0, delta), t, speed, scale, warp, vorticity);
    return vec2(dx, dy) / (2.0 * delta);
}


void main() {
    vec2 uv = qt_TexCoord0 * 2.0 - 1.0;
    float dist = length(uv);

    float edgeSoftness = 0.015;
    float circleMask = 1.0 - smoothstep(1.0 - edgeSoftness, 1.0, dist);

    if (dist > 1.0) {
        fragColor = vec4(0.0);
        return;
    }

    float speed    = max(uFlowSpeed, 0.01);
    float scale    = max(uScale, 0.01);
    float warp     = clamp(uWarpStrength, 0.0, 2.0);
    float detail   = clamp(uDetail, 1.0, float(MAX_OCTAVES));
    float vort     = clamp(uVorticity, 0.0, 1.5);
    float normS    = clamp(uNormalStrength, 0.0, 0.3);
    float contrast = max(uCloudContrast, 0.1);
    float coverage = clamp(uCloudCoverage, 0.05, 0.95);
    float speedAmt  = clamp(uSpeedColorAmount, 0.0, 2.0);
    float shadeAmt  = clamp(uShadingStrength, 0.0, 1.0);

    float timeWarp = sin(uTime * 0.1) * 0.5 + cos(uTime * 0.25) * 0.5;
    float effectiveTime = uTime * speed + timeWarp * 10.0; 

    float levelCurve = smoothstep(0.0, 1.0, uLevel);
    float turbulence = 0.85 + levelCurve * 0.3 + uPulse * 0.08;

    // 主云团（大尺度分布，决定"哪里有云、哪里空白"）
    float rawMain = getRawDensity(uv, effectiveTime, speed, scale, warp, detail, vort);
    float cloudMask = toCloudMask(rawMain, contrast, coverage);

    // 蓝青色云团：用不同相位、独立坐标偏移，覆盖整个圆而不是只在中心
    vec2 cyanUv = uv * 0.9 + vec2(4.1, -2.7); // 常数偏移让它和主云团错开，产生层次
    float cyanT = effectiveTime * speed * 0.8;
    cyanUv.x += cyanT * 0.15;
    cyanUv.y -= cos(effectiveTime * speed * 0.4) * 0.08;
    float rawCyan = fluidPattern(cyanUv, cyanT, scale * 0.85, warp, detail, vort);
    float cyanMask = toCloudMask(rawCyan, contrast * 0.9, coverage + 0.03);

    // 让蓝色云团叠加在主云团之上，两者相乘制造"云中有更深蓝斑"的层次，而不是径向强制聚拢
    float cyanCloud = cyanMask * cloudMask * turbulence;

    // 颜色混合：整体云雾浓度决定基础色阶
    float colorMix = clamp(cloudMask * turbulence, 0.0, 1.0);

    vec3 baseColor;
    if (colorMix < 0.5) {
        baseColor = mix(uColorLight, uColorMid, colorMix * 2.0);
    } else {
        baseColor = mix(uColorMid, uColorDeep, (colorMix - 0.5) * 2.0);
    }

    // ---- 法线明暗：只做很轻的辅助质感，不再主导画面 ----
    // ---- 用梯度模长(局部"流速")驱动颜色偏青 ----
    float bigDelta = (2.0 / max(uResolution.x, 1.0)) * 9.0;
    vec2 coarseGrad = getCoarseGrad(uv, bigDelta, uTime, speed, scale, warp, vort);
    float flowSpeedLocal = length(coarseGrad);

    float softness = clamp(uSpeedSoftness, 0.05, 1.5);
    float speedNorm = smoothstep(0.12, 0.12 + 0.9 * softness, flowSpeedLocal) * speedAmt;
    speedNorm = clamp(speedNorm, 0.0, 1.0);
    speedNorm *= cloudMask;

    vec3 targetCyan = mix(vec3(0.0, 0.85, 0.95), uColorMid, colorMix * 0.35);

    baseColor = mix(baseColor, uColorFast, speedNorm * 0.5);
    targetCyan = mix(targetCyan, uColorFast, speedNorm * 0.4);

    vec3 blendColor = targetCyan * cyanCloud;
    vec3 finalColor = 1.0 - (1.0 - baseColor) * (1.0 - blendColor);
    
    vec3 n = normalize(vec3(-coarseGrad * normS * 3.0, 1.0));
    vec3 lightDir = normalize(vec3(-0.4, 0.55, 0.7));
    float diff = clamp(dot(n, lightDir), 0.0, 1.0);
    float fineShading = 0.94 + 0.12 * diff;

    float depthShading = mix(1.08, 0.90, cloudMask) ; // 云薄->偏亮，云厚->偏暗，制造体积感
    depthShading = mix(1.0, depthShading, shadeAmt);   // 用 uShadingStrength 控制这个效果的强弱
    finalColor *= fineShading * depthShading;


    // 边缘柔光
    float rim = smoothstep(0.88, 0.99, dist);
    vec3 rimLightColor = mix(targetCyan, uColorLight, 0.3);
    finalColor = mix(finalColor, rimLightColor, rim * 0.12);

    // 顶部高光
    vec2 highlightCenter = vec2(-0.35, 0.4);
    float highlightDist = length(uv - highlightCenter);
    float highlight = smoothstep(0.7, 0.0, highlightDist) * 0.25;
    finalColor = mix(finalColor, uColorLight, highlight);

    // fragColor = vec4(vec3(cloudMask), 1.0) * circleMask;
    // return;

    fragColor = vec4(finalColor * circleMask, circleMask) * qt_Opacity;
}