#version 150
// FOREGROUND: the tracked visitor, CLEAR & sharp (real mirrored video) masked by their
// SUBJECT mask (silhouette ∩ DNN person boxes -> ignores background clutter). Edge kept
// delicate: a thin crisp rim + a slow scan-line sweeping down the body (classy, not heavy).
uniform sampler2D uCam;
uniform sampler2D uSil;      // subject mask
uniform vec2  uTexel;
uniform float uTime;
in  vec2 vUv;
out vec4 outColor;

void main(){
    float s = texture(uSil, vUv).r;
    float a = smoothstep(0.32, 0.62, s);

    // thin, crisp rim (much subtler than before)
    float sx = texture(uSil, vUv + vec2(uTexel.x, 0.0)).r - texture(uSil, vUv - vec2(uTexel.x, 0.0)).r;
    float sy = texture(uSil, vUv + vec2(0.0, uTexel.y)).r - texture(uSil, vUv - vec2(0.0, uTexel.y)).r;
    float edge = clamp(length(vec2(sx, sy)) * 1.1, 0.0, 1.0);
    edge *= edge;                                       // crisper / thinner

    if (a < 0.02 && edge < 0.06) discard;               // background effect shows through

    vec3 c = texture(uCam, vUv).rgb;                    // clear, real, sharp

    float ly   = fract(uTime * 0.07);                   // slow scan sweep down the body
    float band = smoothstep(0.045, 0.0, abs(vUv.y - ly));
    c += vec3(0.30, 0.70, 0.85) * band * a * 0.45;      // faint cyan scan light

    c += vec3(0.14, 0.52, 0.66) * edge * 0.5;           // delicate rim

    outColor = vec4(c, max(a, edge * 0.5));
}
