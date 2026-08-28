#version 150

// Global GLITCH / high-tech post pass applied over EVERY effect: RGB channel split,
// block-displacement tearing, scanlines, chromatic aberration, digital grain and
// vignette. Intensity scales with hand motion + scene-change bursts (interactive).
uniform sampler2D uCol;
uniform vec2  uRes;
uniform float uTime;
uniform float uAmt;      // 0..1 glitch intensity

in  vec2 vUv;
out vec4 outColor;

float hash(vec2 p){ return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }

void main(){
    vec2 uv = vUv;
    float amt = clamp(uAmt, 0.0, 1.0);

    // horizontal block tearing in random bands (time-quantised)
    float t2 = floor(uTime * 14.0);
    float band = floor(uv.y * 30.0);
    float g = step(0.85 - 0.30 * amt, hash(vec2(band, t2)));
    float shift = (hash(vec2(band + 7.0, t2)) - 0.5) * (0.03 + 0.14 * amt) * g;
    uv.x = fract(uv.x + shift);

    // fine rolling wave displacement
    uv.x += sin(uv.y * 60.0 + uTime * 3.0) * 0.0015 * (0.4 + amt);

    // chromatic aberration / RGB split
    float ca = (0.0018 + 0.010 * amt) * (0.6 + 0.4 * sin(uTime * 2.3 + uv.y * 8.0));
    float r = texture(uCol, uv + vec2( ca, 0.0)).r;
    float gc= texture(uCol, uv).g;
    float b = texture(uCol, uv + vec2(-ca, 0.0)).b;
    vec3 col = vec3(r, gc, b);

    // occasional bright "data" scanline slice
    float slice = step(0.995 - 0.02 * amt, hash(vec2(floor(uv.y * 220.0), t2)));
    col += slice * 0.25 * amt;

    // scanlines
    col *= 0.86 + 0.14 * sin(uv.y * uRes.y * 1.6);

    // digital grain
    col += (hash(uv * uRes + uTime) - 0.5) * (0.04 + 0.05 * amt);

    // vignette
    vec2 q = uv - 0.5;
    col *= mix(0.55, 1.0, smoothstep(0.95, 0.30, length(q)));

    outColor = vec4(col, 1.0);
}
