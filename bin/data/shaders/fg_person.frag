#version 150
// FOREGROUND: the tracked visitor, shown CLEAR and sharp (real mirrored video) masked by
// their silhouette, with a cyan "scanned subject" rim light. The changing effect lives
// BEHIND this — so people always clearly see themselves in front.
uniform sampler2D uCam;
uniform sampler2D uSil;
uniform vec2  uTexel;     // 1 / silhouette resolution
uniform float uTime;
in  vec2 vUv;
out vec4 outColor;

void main(){
    float s  = texture(uSil, vUv).r;
    float a  = smoothstep(0.30, 0.60, s);                 // feathered fill

    // silhouette gradient -> rim outline
    float sx = texture(uSil, vUv + vec2(uTexel.x, 0.0)).r - texture(uSil, vUv - vec2(uTexel.x, 0.0)).r;
    float sy = texture(uSil, vUv + vec2(0.0, uTexel.y)).r - texture(uSil, vUv - vec2(0.0, uTexel.y)).r;
    float edge = clamp(length(vec2(sx, sy)) * 2.2, 0.0, 1.0);

    if (a < 0.02 && edge < 0.04) discard;                 // background shows through everywhere else

    vec3 c = texture(uCam, vUv).rgb;                      // clear, real, sharp
    float pulse = 0.75 + 0.25 * sin(uTime * 3.0 + vUv.y * 18.0);
    c += vec3(0.10, 0.75, 0.95) * edge * 1.4 * pulse;     // cyan scan rim

    outColor = vec4(c, max(a, edge * 0.9));
}
