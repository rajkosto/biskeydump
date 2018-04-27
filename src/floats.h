#include "hwinit/types.h"
#include "lib/fixmath/fixmath.h"

static inline fix16_t sign(fix16_t x)
{
    if (x < 0)
        return F16(-1);
    else if (x > 0)
        return F16(1);
    else
        return F16(0);
}

static inline fix16_t min(fix16_t x, fix16_t y)
{
    return fix16_min(x,y);
}

static inline fix16_t max(fix16_t x, fix16_t y)
{
    return fix16_max(x,y);
}

static inline fix16_t clamp(fix16_t x, fix16_t minVal, fix16_t maxVal)
{
    return fix16_clamp(x, minVal, maxVal);
}

static inline fix16_t smoothstep(fix16_t edge0, fix16_t edge1, fix16_t x)
{
    fix16_t t = clamp(fix16_div(fix16_sub(x,edge0),fix16_sub(edge1,edge0)), F16(0.0), F16(1.0));
    return fix16_mul(fix16_mul(t,t),fix16_sub(F16(3.0),fix16_mul(F16(2.0),t)));
}

static inline fix16_t mix(fix16_t x, fix16_t y, fix16_t a)
{
    fix16_t negA = fix16_sub(F16(1.0),a);
    return fix16_add(fix16_mul(x,negA),fix16_mul(y,a));
}

typedef struct { fix16_t x,y; } vec2;
typedef struct { fix16_t x,y,z; } vec3;
typedef struct { fix16_t x,y,z,w; } vec4;

static inline vec2 get_xy(vec4 src) { return (vec2){src.x, src.y}; }
static inline vec2 get_zw(vec4 src) { return (vec2){src.z, src.w}; }
static inline vec3 get_xyz(vec4 src) { return (vec3){src.x, src.y, src.z}; }
static inline void apply_xyz(vec4* tgt, vec3 src) { tgt->x = src.x; tgt->y = src.y; tgt->z = src.z; }

static inline vec2 vec2_add(vec2 a, vec2 b) { return (vec2){fix16_add(a.x,b.x), fix16_add(a.y,b.y)}; }
static inline vec2 vec2_add_one(vec2 a, fix16_t b) { return (vec2){fix16_add(a.x,b), fix16_add(a.y,b)}; }
static inline vec2 vec2_sub(vec2 a, vec2 b) { return (vec2){fix16_sub(a.x,b.x), fix16_sub(a.y,b.y)}; }
static inline vec2 vec2_sub_one(vec2 a, fix16_t b) { return (vec2){fix16_sub(a.x,b), fix16_sub(a.y,b)}; }
static inline vec2 vec2_mul(vec2 a, vec2 b) { return (vec2){fix16_mul(a.x,b.x), fix16_mul(a.y,b.y)}; }
static inline vec2 vec2_mul_one(vec2 a, fix16_t b) { return (vec2){fix16_mul(a.x,b), fix16_mul(a.y,b)}; }
static inline vec2 vec2_div(vec2 a, vec2 b) { return (vec2){fix16_div(a.x,b.x), fix16_div(a.y,b.y)}; }
static inline vec2 vec2_div_one(vec2 a, fix16_t b) { return (vec2){fix16_div(a.x,b), fix16_div(a.y,b)}; }
static inline fix16_t vec2_lengthsq(vec2 a) { return fix16_add(fix16_mul(a.x,a.x),fix16_mul(a.y,a.y)); }
static inline fix16_t vec2_length(vec2 a) { return fix16_sqrt(vec2_lengthsq(a)); }

static inline fix16_t vec2_dot(vec2 a, vec2 b) { return fix16_add(fix16_mul(a.x,b.x),fix16_mul(a.y,b.y)); }

static inline vec3 vec3_add(vec3 a, vec3 b) { return (vec3){fix16_add(a.x,b.x), fix16_add(a.y,b.y), fix16_add(a.z,b.z)}; }
static inline vec3 vec3_add_one(vec3 a, fix16_t b) { return (vec3){fix16_add(a.x,b), fix16_add(a.y,b), fix16_add(a.z,b)}; }
static inline vec3 vec3_sub(vec3 a, vec3 b) { return (vec3){fix16_sub(a.x,b.x), fix16_sub(a.y,b.y), fix16_sub(a.z,b.z)}; }
static inline vec3 vec3_sub_one(vec3 a, fix16_t b) { return (vec3){fix16_sub(a.x,b), fix16_sub(a.y,b), fix16_sub(a.z,b)}; }
static inline vec3 vec3_mul(vec3 a, vec3 b) { return (vec3){fix16_mul(a.x,b.x), fix16_mul(a.y,b.y), fix16_mul(a.z,b.z)}; }
static inline vec3 vec3_mul_one(vec3 a, fix16_t b) { return (vec3){fix16_mul(a.x,b), fix16_mul(a.y,b), fix16_mul(a.z,b)}; }
static inline vec3 vec3_div(vec3 a, vec3 b) { return (vec3){fix16_div(a.x,b.x), fix16_div(a.y,b.y), fix16_div(a.z,b.z)}; }
static inline vec3 vec3_div_one(vec3 a, fix16_t b) { return (vec3){fix16_div(a.x,b), fix16_div(a.y,b), fix16_div(a.z,b)}; }
static inline fix16_t vec3_lengthsq(vec3 a) { return fix16_add(fix16_add(fix16_mul(a.x,a.x),fix16_mul(a.y,a.y)),fix16_mul(a.z,a.z)); }
static inline fix16_t vec3_length(vec3 a) { return fix16_sqrt(vec3_lengthsq(a)); }

static inline vec3 vec3_mix(vec3 x, vec3 y, fix16_t a)
{
    fix16_t negA = fix16_sub(F16(1.0),a);
    return vec3_add(vec3_mul_one(x,negA),vec3_mul_one(y,a));
}

static inline vec4 vec4_add(vec4 a, vec4 b) { return (vec4){fix16_add(a.x,b.x), fix16_add(a.y,b.y), fix16_add(a.z,b.z), fix16_add(a.w,b.w)}; }
static inline vec4 vec4_add_one(vec4 a, fix16_t b) { return (vec4){fix16_add(a.x,b), fix16_add(a.y,b), fix16_add(a.z,b), fix16_add(a.w,b)}; }
static inline vec4 vec4_sub(vec4 a, vec4 b) { return (vec4){fix16_sub(a.x,b.x), fix16_sub(a.y,b.y), fix16_sub(a.z,b.z), fix16_sub(a.w,b.w)}; }
static inline vec4 vec4_sub_one(vec4 a, fix16_t b) { return (vec4){fix16_sub(a.x,b), fix16_sub(a.y,b), fix16_sub(a.z,b), fix16_sub(a.w,b)}; }
static inline vec4 vec4_mul(vec4 a, vec4 b) { return (vec4){fix16_mul(a.x,b.x), fix16_mul(a.y,b.y), fix16_mul(a.z,b.z), fix16_mul(a.w,b.w)}; }
static inline vec4 vec4_mul_one(vec4 a, fix16_t b) { return (vec4){fix16_mul(a.x,b), fix16_mul(a.y,b), fix16_mul(a.z,b), fix16_mul(a.w,b)}; }
static inline vec4 vec4_div(vec4 a, vec4 b) { return (vec4){fix16_div(a.x,b.x), fix16_div(a.y,b.y), fix16_div(a.z,b.z), fix16_div(a.w,b.w)}; }
static inline vec4 vec4_div_one(vec4 a, fix16_t b) { return (vec4){fix16_div(a.x,b), fix16_div(a.y,b), fix16_div(a.z,b), fix16_div(a.w,b)}; }
static inline fix16_t vec4_lengthsq(vec4 a) { return fix16_add(fix16_add(fix16_mul(a.x,a.x),fix16_mul(a.y,a.y)),fix16_add(fix16_mul(a.z,a.z),fix16_mul(a.w,a.w))); }
static inline fix16_t vec4_length(vec4 a) { return fix16_sqrt(vec4_lengthsq(a)); }

static inline vec4 vec4_mix(vec4 x, vec4 y, fix16_t a)
{
    fix16_t negA = fix16_sub(F16(1.0),a);
    return vec4_add(vec4_mul_one(x,negA),vec4_mul_one(y,a));
}

static inline u32 fixeds_to_pixel(fix16_t fR, fix16_t fG, fix16_t fB)
{
    int iR, iG, iB;
    iR = fix16_mul(fR,F16(255.0)) >> 16;
    iG = fix16_mul(fG,F16(255.0)) >> 16;
    iB = fix16_mul(fB,F16(255.0)) >> 16;

    if (iR < 0) iR = 0; else if (iR > 255) iR = 255;
    if (iG < 0) iG = 0; else if (iG > 255) iG = 255;
    if (iB < 0) iB = 0; else if (iB > 255) iB = 255;
    
    u32 out = 0;
    out |= iR << 0;
    out |= iG << 8;
    out |= iB << 16;

    return out;
}