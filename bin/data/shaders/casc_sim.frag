#version 150
// FLOOR CASCADE — the visitor's data "falls" off the wall and rains down the floor:
// glyphs fall (layered speeds), sway like drifting ash, and gently SLOW near the floor
// (settling into a soft sediment) but keep flowing so they recycle — no hard pile-up.
// Spawns bias toward the person's column so it reads as THEIR data spilling down.
uniform sampler2D uState;
uniform float uTime;
uniform float uDt;
uniform float uEnergy;      // person motion 0..1 -> heavier rain
uniform float uHotX[8];     // person column(s), 0..1 (mirrored screen space)
uniform int   uHotN;
in  vec2 vUv;
out vec4 o;
float hash(vec2 p){ return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }

void main(){
    vec4 s = texture(uState, vUv);
    float x = s.r, y = s.g, life = s.b, seed = s.a;

    float band = smoothstep(0.45, 0.96, y);            // 0 near top .. 1 near floor (SMOOTH — no seam line)
    float lane = 0.55 + 0.9 * seed;
    float fallTop = (0.15 + 0.10 * uEnergy) * lane;
    float fallBot = 0.050 + 0.03 * seed;               // slows but keeps flowing -> exits & recycles
    float fall = mix(fallTop, fallBot, band);
    y += fall * uDt;

    float sway = sin(y * 6.0 + uTime * 0.6 + seed * 6.283)
               + (hash(vec2(seed * 51.0, floor(uTime * 1.6))) - 0.5);
    float amp  = mix(0.016, 0.038, band);
    x += sway * amp * uDt;
    x = clamp(x, 0.0, 1.0);

    life += uDt;
    float maxLife = 6.0 + seed * 4.0;                  // shorter -> sediment self-limits, stays airy

    if (y > 1.03 || life > maxLife) {                  // recycle to just above the top
        float r1 = hash(vec2(vUv.x * 137.1 + uTime * 11.3, vUv.y * 59.7 - uTime * 6.1));
        float r2 = hash(vec2(vUv.y *  41.7 - uTime *  4.3, vUv.x * 83.9 + uTime * 7.9));
        float nx = r1;
        if (uHotN > 0 && r2 < 0.55) {                  // bias to the person's column(s)
            int hi = int(r1 * float(uHotN)) % uHotN;
            nx = clamp(uHotX[hi] + (r2 - 0.5) * 0.16, 0.0, 1.0);
        }
        x = nx; y = -0.03 - r2 * 0.08; life = 0.0; seed = r2;
    }
    o = vec4(x, y, life, seed);
}
