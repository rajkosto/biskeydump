#include "floats.h"

// "Smiley Tutorial" by Martijn Steinrucken aka BigWings - 2017
// License Creative Commons Attribution-NonCommercial-ShareAlike 3.0 Unported License.
//
// This Smiley is part of my ShaderToy Tutorial series on YouTube:
// Part 1 - Creating the Smiley - https://www.youtube.com/watch?v=ZlNnrpM0TRg
// Part 2 - Animating the Smiley - https://www.youtube.com/watch?v=vlD_KOrzGDc&t=83s

#define S(a, b, t) smoothstep(a, b, t)
#define B(a, b, blur, t) S(a-blur, a+blur, t)*S(b+blur, b-blur, t)
#define sat(x) clamp(x, 0.0f, 1.0f)

inline float remap01(float a, float b, float t) {
	return sat((t-a)/(b-a));
}

inline float remap(float a, float b, float c, float d, float t) {
	return sat((t-a)/(b-a)) * (d-c) + c;
}

inline vec2 within(vec2 uv, vec4 rect) {
	return vec2_div(vec2_sub(uv,get_xy(rect)),vec2_sub(get_zw(rect),get_xy(rect)));
}

inline vec4 Brow(vec2 uv, float smile) {
    float offs = mix(0.2f, 0.0f, smile);
    uv.y += offs;
    
    float y = uv.y;
    uv.y += uv.x*mix(0.5f, 0.8f, smile)-mix(0.1f, 0.3f, smile);
    uv.x -= mix(0.0f, 0.1f, smile);
    uv = vec2_sub_one(uv, 0.5f);
    
    vec4 col = {0.0f, 0.0f, 0.0f, 0.0f};
    
    float blur = 0.1f;
    
   	float d1 = vec2_length(uv);
    float s1 = S(0.45f, 0.45f-blur, d1);
    float d2 = vec2_length(vec2_sub(uv,vec2_mul_one((vec2){0.1f, -0.2f},0.7f)));
    float s2 = S(0.5f, 0.5f-blur, d2);
    
    float browMask = sat(s1-s2);
    
    float colMask = remap01(0.7f, 0.8f, y)*0.75f;
    colMask *= S(.6, .9, browMask);
    colMask *= smile;
    vec4 browCol = vec4_mix((vec4){0.4f, 0.2f, 0.2f, 1.0f}, (vec4){1.0f, 0.75f, 0.5f, 1.0f}, colMask); 
   
    uv.y += 0.15f-offs*0.5f;
    blur += mix(0.0f, 0.1f, smile);
    d1 = vec2_length(uv);
    s1 = S(0.45f, 0.45f-blur, d1);
    d2 = vec2_length(vec2_sub(uv,vec2_mul_one((vec2){0.1f, -0.2f},0.7f)));
    s2 = S(0.5f, 0.5f-blur, d2);
    float shadowMask = sat(s1-s2);
    
    col = vec4_mix(col, (vec4){0.0f,0.0f,0.0f,1.0f}, S(0.0f, 1.0f, shadowMask)*0.5f);
    
    col = vec4_mix(col, browCol, S(0.2f, 0.4f, browMask));
    
    return col;
}

inline vec4 Eye(vec2 uv, float side, vec2 m, float smile, float eyeTime) {
    uv = vec2_sub_one(uv,0.5f);
    uv.x *= side;
    
	float d = vec2_length(uv);
    vec4 irisCol = {0.3f, 0.5f, 1.0f, 1.0f};
    vec4 col = vec4_mix((vec4){1.0f, 1.0f, 1.0f, 1.0f}, irisCol, S(0.1f, 0.7f, d)*0.5f);		// gradient in eye-white
    col.w = S(0.5f, 0.48f, d);									// eye mask
    
    apply_xyz(&col, vec3_mul_one(get_xyz(col), 1.0f - S(0.45f, 0.5f, d)*0.5f*sat(-uv.y-uv.x*side))); 	// eye shadow
    
    d = vec2_length(vec2_sub(uv,vec2_mul_one(m,0.4f)));									// offset iris pos to look at mouse cursor
    apply_xyz(&col,vec3_mix(get_xyz(col), (vec3){0.0f,0.0f,0.0f}, S(0.3f, 0.28f, d))); 		// iris outline
    
    apply_xyz(&irisCol,vec3_mul_one(get_xyz(irisCol), 1.0f + S(0.3f, 0.05f, d)));						// iris lighter in center
    float irisMask = S(0.28f, 0.25f, d);
    apply_xyz(&col,vec3_mix(get_xyz(col), get_xyz(irisCol), irisMask));			// blend in iris
    
    d = vec2_length(vec2_sub(uv,vec2_mul_one(m,0.45f)));									// offset pupil to look at mouse cursor
    
    float pupilSize = mix(0.4f, 0.16f, smile);
    float pupilMask = S(pupilSize, pupilSize*0.85f, d);
    pupilMask *= irisMask;
    apply_xyz(&col,vec3_mix(get_xyz(col), (vec3){0.0f, 0.0f, 0.0f}, pupilMask));		// blend in pupil
    
    vec2 offs = {sinf(eyeTime+uv.y*25.0f), sinf(eyeTime+uv.x*25.0f)};
    offs = vec2_mul_one(offs,0.01f*(1.0f-smile));
    
    uv = vec2_add(uv,offs);
    float highlight = S(0.1f, 0.09f, vec2_length(vec2_sub(uv,(vec2){-0.15f, 0.15f})));
    highlight += S(0.07f, 0.05f, vec2_length(vec2_add(uv,(vec2){-0.08f, 0.08f})));
    apply_xyz(&col,vec3_mix(get_xyz(col), (vec3){1.0f, 1.0f, 1.0f}, highlight));			// blend in highlight
    
    return col;
}

inline vec4 Mouth(vec2 uv, float smile) {
    uv = vec2_sub_one(uv,0.5f);
	vec4 col = {0.5f, 0.18f, 0.05f, 1.0f};
    
    uv.y *= 1.5f;
    uv.y -= uv.x*uv.x*2.0f*smile;
    
    uv.x *= mix(2.5f, 1.0f, smile);
    
    float d = vec2_length(uv);
    col.w = S(0.5f, 0.48f, d);
    
    vec2 tUv = uv;
    tUv.y += (fabs(uv.x)*0.5f+0.1f)*(1.0f-smile);
    float td = vec2_length(vec2_sub(tUv,(vec2){0.0f, 0.6f}));
    
    vec3 toothCol = vec3_mul_one((vec3){1.0f, 1.0f, 1.0f},S(0.6f, 0.35f, d));
    apply_xyz(&col,vec3_mix(get_xyz(col), toothCol, S(0.4f, 0.37f, td)));
    
    td = vec2_length(vec2_add(uv,(vec2){0.0f, 0.5f}));
    apply_xyz(&col,vec3_mix(get_xyz(col), (vec3){1.0f, 0.5f, 0.5f}, S(0.5f, 0.2f, td)));
    return col;
}

inline vec4 Head(vec2 uv) {
	vec4 col = {0.9f, 0.65f, 0.1f, 1.0f};
    
    float d = vec2_length(uv);
    
    col.w = S(0.5f, 0.49f, d);
    
    float edgeShade = remap01(0.35f, 0.5f, d);
    edgeShade *= edgeShade;
    apply_xyz(&col,vec3_mul_one(get_xyz(col),1.0f-edgeShade*0.5f));
    
    apply_xyz(&col,vec3_mix(get_xyz(col), (vec3){0.6f, 0.3f, 0.1f}, S(0.47f, 0.48f, d)));
    
    float highlight = S(0.41f, 0.405, d);
    highlight *= remap(0.41f, -0.1f, 0.75f, 0.0f, uv.y);
    highlight *= S(0.18f, 0.19f, vec2_length(vec2_sub(uv,(vec2){0.21f, 0.08f})));
    apply_xyz(&col,vec3_mix(get_xyz(col), (vec3){1.0f, 1.0f, 1.0f}, highlight));
    
    d = vec2_length(vec2_sub(uv,(vec2){0.25f, -0.2f}));
    float cheek = S(0.2f,0.01f, d)*0.4f;
    cheek *= S(0.17f, 0.16f, d);
    apply_xyz(&col,vec3_mix(get_xyz(col), (vec3){1.0f, 0.1f, 0.1f}, cheek));
    
    return col;
}

inline vec4 Smiley(vec2 uv, vec2 m, float smile, float eyeTime) {
	vec4 col = {0.0f,0.0f,0.0f,0.0f};
    
    if(vec2_lengthsq(uv) < 0.25f) // only bother about pixels that are actually inside the head
    {
        float side = sign(uv.x);
        uv.x = fabs(uv.x);
        vec4 head = Head(uv);
        col = vec4_mix(col, head, head.w);

        if(vec2_lengthsq(vec2_sub(uv,(vec2){0.2f, 0.075f})) < 0.030625f) {
            vec4 eye = Eye(within(uv, (vec4){0.03f, -0.1f, 0.37f, 0.25f}), side, m, smile, eyeTime);
            col = vec4_mix(col, eye, eye.w);
        }
        else if(vec2_lengthsq(vec2_sub(uv,(vec2){0.0f, -0.15f})) < 0.09f) {
            vec4 mouth = Mouth(within(uv, (vec4){-0.3f, -0.43f, 0.3f, -0.13f}), smile);
            col = vec4_mix(col, mouth, mouth.w);
        }
        else if(vec2_lengthsq(vec2_sub(uv,(vec2){0.185f, 0.325f})) < 0.0324f) {
            vec4 brow = Brow(within(uv, (vec4){0.03f, 0.2f, 0.4f, 0.45f}), smile);
            col = vec4_mix(col, brow, brow.w);
        }
    }
    
    return col;
}

vec4 mainImage( vec2 uv, vec2 m, float smile, float eyeTime )
{
    float d = vec2_dot(uv, uv);
    uv= vec2_sub(uv, vec2_mul_one(m,sat(0.23f-d)));
    
    vec4 fragColor = Smiley(uv, m, smile, eyeTime);
    return fragColor;
}