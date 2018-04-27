#include "hwinit/types.h"
#include <math.h>

inline float sign(float x)
{
    if (x < 0)
        return -1;
    else if (x > 0)
        return 1;
    else
        return 0;
}

inline float min(float x, float y)
{
    if (y < x)
        return y;
    else
        return x;
}

inline float max(float x, float y)
{
    if (y > x)
        return y;
    else
        return x;
}

inline float clamp(float x, float minVal, float maxVal)
{
    return min(max(x, minVal), maxVal);
}

inline float smoothstep(float edge0, float edge1, float x)
{
    float t;
    t = clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

inline float mix(float x, float y, float a)
{
    float negA = 1.0f - a;
    return (x * negA) + (y * a);
}

typedef struct { float x,y; } vec2;
typedef struct { float x,y,z; } vec3;
typedef struct { float x,y,z,w; } vec4;

inline const vec2* get_xy(const vec4* src) { return (const vec2*)(&src->x); }
inline const vec2* get_zw(const vec4* src) { return (const vec2*)(&src->z); }
inline const vec3* get_xyz(const vec4* src) { return (const vec3*)(&src->x); }

inline vec2 vec2_add(const vec2* a, const vec2* b) { return (vec2){a->x+b->x, a->y+b->y}; }
inline vec2 vec2_add_one(const vec2* a, float b) { return (vec2){a->x+b, a->y+b}; }
inline vec2 vec2_sub(const vec2* a, const vec2* b) { return (vec2){a->x-b->x, a->y-b->y}; }
inline vec2 vec2_sub_one(const vec2* a, float b) { return (vec2){a->x-b, a->y-b}; }
inline vec2 vec2_mul(const vec2* a, const vec2* b) { return (vec2){a->x*b->x, a->y*b->y}; }
inline vec2 vec2_mul_one(const vec2* a, float b) { return (vec2){a->x*b, a->y*b}; }
inline vec2 vec2_div(const vec2* a, const vec2* b) { return (vec2){a->x/b->x, a->y/b->y}; }
inline vec2 vec2_div_one(const vec2* a, float b) { return (vec2){a->x/b, a->y/b}; }
inline float vec2_lengthsq(const vec2* a) { return a->x*a->x + a->y*a->y; }
inline float vec2_length(const vec2* a) { return sqrtf(vec2_lengthsq(a)); }

inline float vec2_dot(const vec2* a, const vec2* b) { return a->x*b->x + a->y*b->y; }

inline vec3 vec3_add(const vec3* a, const vec3* b) { return (vec3){a->x+b->x, a->y+b->y, a->z+b->z}; }
inline vec3 vec3_add_one(const vec3* a, float b) { return (vec3){a->x+b, a->y+b, a->z+b}; }
inline vec3 vec3_sub(const vec3* a, const vec3* b) { return (vec3){a->x-b->x, a->y-b->y, a->z-b->z}; }
inline vec3 vec3_sub_one(const vec3* a, float b) { return (vec3){a->x-b, a->y-b, a->z-b}; }
inline vec3 vec3_mul(const vec3* a, const vec3* b) { return (vec3){a->x*b->x, a->y*b->y, a->z*b->z}; }
inline vec3 vec3_mul_one(const vec3* a, float b) { return (vec3){a->x*b, a->y*b, a->z*b}; }
inline vec3 vec3_div(const vec3* a, const vec3* b) { return (vec3){a->x/b->x, a->y/b->y, a->z/b->z}; }
inline vec3 vec3_div_one(const vec3* a, float b) { return (vec3){a->x/b, a->y/b, a->z/b}; }
inline float vec3_lengthsq(const vec3* a) { return a->x*a->x + a->y*a->y + a->z*a->z; }
inline float vec3_length(const vec3* a) { return sqrtf(vec3_lengthsq(a)); }

inline vec3 vec3_mix(const vec3* x, const vec3* y, float a)
{
    float negA = 1.0f - a;
    vec3 scaledX = vec3_mul_one(x,negA);
    vec3 scaledY = vec3_mul_one(y,a);
    return vec3_add(&scaledX,&scaledY);
}

inline vec4 vec4_add(const vec4* a, const vec4* b) { return (vec4){a->x+b->x, a->y+b->y, a->z+b->z, a->w+b->w}; }
inline vec4 vec4_add_one(const vec4* a, float b) { return (vec4){a->x+b, a->y+b, a->z+b, a->w+b}; }
inline vec4 vec4_sub(const vec4* a, const vec4* b) { return (vec4){a->x-b->x, a->y-b->y, a->z-b->z, a->w-b->w}; }
inline vec4 vec4_sub_one(const vec4* a, float b) { return (vec4){a->x-b, a->y-b, a->z-b, a->w-b}; }
inline vec4 vec4_mul(const vec4* a, const vec4* b) { return (vec4){a->x*b->x, a->y*b->y, a->z*b->z, a->w*b->w}; }
inline vec4 vec4_mul_one(const vec4* a, float b) { return (vec4){a->x*b, a->y*b, a->z*b, a->w*b}; }
inline vec4 vec4_div(const vec4* a, const vec4* b) { return (vec4){a->x/b->x, a->y/b->y, a->z/b->z, a->w/b->w}; }
inline vec4 vec4_div_one(const vec4* a, float b) { return (vec4){a->x/b, a->y/b, a->z/b, a->w/b}; }
inline float vec4_lengthsq(const vec4* a) { return a->x*a->x + a->y*a->y + a->z*a->z + a->w*a->w; }
inline float vec4_length(const vec4* a) { return sqrtf(vec4_lengthsq(a)); }

inline vec4 vec4_mix(const vec4* x, const vec4* y, float a)
{
    float negA = 1.0f - a;
    vec4 scaledX = vec4_mul_one(x,negA);
    vec4 scaledY = vec4_mul_one(y,a);
    return vec4_add(&scaledX,&scaledY);
}

extern float g_iTime;
inline u32 floats_to_pixel(float fR, float fG, float fB)
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