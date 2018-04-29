#include "hwinit/types.h"
#include "utils.h"
#include <math.h>

inline ALINLINE float sign(float x)
{
    if (x < 0)
        return -1;
    else if (x > 0)
        return 1;
    else
        return 0;
}

inline ALINLINE float min(float x, float y)
{
    if (y < x)
        return y;
    else
        return x;
}

inline ALINLINE float max(float x, float y)
{
    if (y > x)
        return y;
    else
        return x;
}

inline ALINLINE float clamp(float x, float minVal, float maxVal)
{
    return min(max(x, minVal), maxVal);
}

inline ALINLINE float smoothstep(float edge0, float edge1, float x)
{
    float t;
    t = clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

inline ALINLINE float mix(float x, float y, float a)
{
    float negA = 1.0f - a;
    return (x * negA) + (y * a);
}

typedef struct { float x,y; } vec2;
typedef struct { float x,y,z; } vec3;
typedef struct { float x,y,z,w; } vec4;

inline ALINLINE vec2 get_xy(vec4 src) { return (vec2){src.x, src.y}; }
inline ALINLINE vec2 get_zw(vec4 src) { return (vec2){src.z, src.w}; }
inline ALINLINE vec3 get_xyz(vec4 src) { return (vec3){src.x, src.y, src.z}; }
inline ALINLINE void apply_xyz(vec4* tgt, vec3 src) { tgt->x = src.x; tgt->y = src.y; tgt->z = src.z; }

inline ALINLINE vec2 vec2_add(vec2 a, vec2 b) { return (vec2){a.x+b.x, a.y+b.y}; }
inline ALINLINE vec2 vec2_add_one(vec2 a, float b) { return (vec2){a.x+b, a.y+b}; }
inline ALINLINE vec2 vec2_sub(vec2 a, vec2 b) { return (vec2){a.x-b.x, a.y-b.y}; }
inline ALINLINE vec2 vec2_sub_one(vec2 a, float b) { return (vec2){a.x-b, a.y-b}; }
inline ALINLINE vec2 vec2_mul(vec2 a, vec2 b) { return (vec2){a.x*b.x, a.y*b.y}; }
inline ALINLINE vec2 vec2_mul_one(vec2 a, float b) { return (vec2){a.x*b, a.y*b}; }
inline ALINLINE vec2 vec2_div(vec2 a, vec2 b) { return (vec2){a.x/b.x, a.y/b.y}; }
inline ALINLINE vec2 vec2_div_one(vec2 a, float b) { return (vec2){a.x/b, a.y/b}; }
inline ALINLINE float vec2_lengthsq(vec2 a) { return a.x*a.x + a.y*a.y; }
inline ALINLINE float vec2_length(vec2 a) { return sqrtf(vec2_lengthsq(a)); }

inline ALINLINE float vec2_dot(vec2 a, vec2 b) { return a.x*b.x + a.y*b.y; }

inline ALINLINE vec3 vec3_add(vec3 a, vec3 b) { return (vec3){a.x+b.x, a.y+b.y, a.z+b.z}; }
inline ALINLINE vec3 vec3_add_one(vec3 a, float b) { return (vec3){a.x+b, a.y+b, a.z+b}; }
inline ALINLINE vec3 vec3_sub(vec3 a, vec3 b) { return (vec3){a.x-b.x, a.y-b.y, a.z-b.z}; }
inline ALINLINE vec3 vec3_sub_one(vec3 a, float b) { return (vec3){a.x-b, a.y-b, a.z-b}; }
inline ALINLINE vec3 vec3_mul(vec3 a, vec3 b) { return (vec3){a.x*b.x, a.y*b.y, a.z*b.z}; }
inline ALINLINE vec3 vec3_mul_one(vec3 a, float b) { return (vec3){a.x*b, a.y*b, a.z*b}; }
inline ALINLINE vec3 vec3_div(vec3 a, vec3 b) { return (vec3){a.x/b.x, a.y/b.y, a.z/b.z}; }
inline ALINLINE vec3 vec3_div_one(vec3 a, float b) { return (vec3){a.x/b, a.y/b, a.z/b}; }
inline ALINLINE float vec3_lengthsq(vec3 a) { return a.x*a.x + a.y*a.y + a.z*a.z; }
inline ALINLINE float vec3_length(vec3 a) { return sqrtf(vec3_lengthsq(a)); }

inline ALINLINE vec3 vec3_mix(vec3 x, vec3 y, float a)
{
    float negA = 1.0f - a;
    return vec3_add(vec3_mul_one(x,negA),vec3_mul_one(y,a));
}

inline ALINLINE vec4 vec4_add(vec4 a, vec4 b) { return (vec4){a.x+b.x, a.y+b.y, a.z+b.z, a.w+b.w}; }
inline ALINLINE vec4 vec4_add_one(vec4 a, float b) { return (vec4){a.x+b, a.y+b, a.z+b, a.w+b}; }
inline ALINLINE vec4 vec4_sub(vec4 a, vec4 b) { return (vec4){a.x-b.x, a.y-b.y, a.z-b.z, a.w-b.w}; }
inline ALINLINE vec4 vec4_sub_one(vec4 a, float b) { return (vec4){a.x-b, a.y-b, a.z-b, a.w-b}; }
inline ALINLINE vec4 vec4_mul(vec4 a, vec4 b) { return (vec4){a.x*b.x, a.y*b.y, a.z*b.z, a.w*b.w}; }
inline ALINLINE vec4 vec4_mul_one(vec4 a, float b) { return (vec4){a.x*b, a.y*b, a.z*b, a.w*b}; }
inline ALINLINE vec4 vec4_div(vec4 a, vec4 b) { return (vec4){a.x/b.x, a.y/b.y, a.z/b.z, a.w/b.w}; }
inline ALINLINE vec4 vec4_div_one(vec4 a, float b) { return (vec4){a.x/b, a.y/b, a.z/b, a.w/b}; }
inline ALINLINE float vec4_lengthsq(vec4 a) { return a.x*a.x + a.y*a.y + a.z*a.z + a.w*a.w; }
inline ALINLINE float vec4_length(vec4 a) { return sqrtf(vec4_lengthsq(a)); }

inline ALINLINE vec4 vec4_mix(vec4 x, vec4 y, float a)
{
    float negA = 1.0f - a;
    return vec4_add(vec4_mul_one(x,negA),vec4_mul_one(y,a));
}

inline ALINLINE u32 floats_to_pixel(float fR, float fG, float fB)
{
    int iR, iG, iB;
    iR = fR * 255.0f;
    iG = fG * 255.0f;
    iB = fB * 255.0f;

    if (iR < 0) iR = 0; else if (iR > 255) iR = 255;
    if (iG < 0) iG = 0; else if (iG > 255) iG = 255;
    if (iB < 0) iB = 0; else if (iB > 255) iB = 255;
    
    u32 out = 0;
    out |= iR << 0;
    out |= iG << 8;
    out |= iB << 16;

    return out;
}