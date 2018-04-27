#include "floats.h"

// "Smiley Tutorial" by Martijn Steinrucken aka BigWings - 2017
// License Creative Commons Attribution-NonCommercial-ShareAlike 3.0 Unported License.
//
// This Smiley is part of my ShaderToy Tutorial series on YouTube:
// Part 1 - Creating the Smiley - https://www.youtube.com/watch?v=ZlNnrpM0TRg
// Part 2 - Animating the Smiley - https://www.youtube.com/watch?v=vlD_KOrzGDc&t=83s

static inline fix16_t S(fix16_t a, fix16_t b, fix16_t t) { return smoothstep(a, b, t); }
static inline fix16_t B(fix16_t a, fix16_t b, fix16_t blur, fix16_t t) 
{ 
    return fix16_mul(   S(fix16_sub(a,blur), fix16_add(a,blur), t),
                        S(fix16_add(b,blur), fix16_sub(b,blur), t)  ); 
}
static inline fix16_t sat(fix16_t x) { return clamp(x, F16(0.0), F16(1.0)); }

static inline fix16_t remap01(fix16_t a, fix16_t b, fix16_t t) {
	return sat(fix16_div(fix16_sub(t,a),fix16_sub(b,a)));
}

static inline fix16_t remap(fix16_t a, fix16_t b, fix16_t c, fix16_t d, fix16_t t) {
	return fix16_add(fix16_mul(remap01(a,b,t),fix16_sub(d,c)),c);
}

static inline vec2 within(vec2 uv, vec4 rect) {
	return vec2_div(vec2_sub(uv,get_xy(rect)),vec2_sub(get_zw(rect),get_xy(rect)));
}

static inline vec4 Brow(vec2 uv, fix16_t smile) {
    fix16_t offs = mix(F16(0.2), F16(0.0), smile);
    uv.y = fix16_add(uv.y,offs);
    
    fix16_t y = uv.y;
    uv.y = fix16_add(uv.y, fix16_mul(uv.x,fix16_sub(mix(F16(0.5), F16(0.8), smile),mix(F16(0.1), F16(0.3), smile))));
    uv.x = fix16_sub(uv.x, mix(F16(0.0), F16(0.1), smile));
    uv = vec2_sub_one(uv, F16(0.5));
    
    vec4 col = {F16(0.0), F16(0.0), F16(0.0), F16(0.0)};
    
    fix16_t blur = F16(0.1);
    
   	fix16_t d1 = vec2_length(uv);
    fix16_t s1 = S(F16(0.45), fix16_sub(F16(0.45),blur), d1);
    fix16_t d2 = vec2_length(vec2_sub(uv,vec2_mul_one((vec2){F16(0.1), F16(-0.2)},F16(0.7))));
    fix16_t s2 = S(F16(0.5), fix16_sub(F16(0.5),blur), d2);
    
    fix16_t browMask = sat(fix16_sub(s1,s2));
    
    fix16_t colMask = fix16_mul(remap01(F16(0.7), F16(0.8), y),F16(0.75));
    colMask = fix16_mul(colMask,S(F16(0.6), F16(0.9), browMask));
    colMask = fix16_mul(colMask,smile);
    vec4 browCol = vec4_mix((vec4){F16(0.4), F16(0.2), F16(0.2), F16(1.0)}, (vec4){F16(1.0), F16(0.75), F16(0.5), F16(1.0)}, colMask); 
   
    uv.y = fix16_add(uv.y,fix16_sub(F16(0.15),fix16_mul(offs,F16(0.5))));
    blur = fix16_add(blur,mix(F16(0.0), F16(0.1), smile));
    d1 = vec2_length(uv);
    s1 = S(F16(0.45), fix16_sub(F16(0.45),blur), d1);
    d2 = vec2_length(vec2_sub(uv,vec2_mul_one((vec2){F16(0.1), F16(-0.2)},F16(0.7))));
    s2 = S(F16(0.5), fix16_sub(F16(0.5),blur), d2);
    fix16_t shadowMask = sat(fix16_sub(s1,s2));
    
    col = vec4_mix(col, (vec4){F16(0.0),F16(0.0),F16(0.0),F16(1.0)}, fix16_mul(S(F16(0.0), F16(1.0), shadowMask),F16(0.5)));
    col = vec4_mix(col, browCol, S(F16(0.2), F16(0.4), browMask));
    
    return col;
}

static inline vec4 Eye(vec2 uv, fix16_t side, vec2 m, fix16_t smile, fix16_t eyeTime) {
    uv = vec2_sub_one(uv,F16(0.5));
    uv.x = fix16_mul(uv.x,side);
    
	fix16_t d = vec2_length(uv);
    vec4 irisCol = {F16(0.3), F16(0.5), F16(1.0), F16(1.0f)};
    vec4 col = vec4_mix((vec4){F16(1.0), F16(1.0), F16(1.0), F16(1.0)}, irisCol, fix16_mul(S(F16(0.1), F16(0.7), d),F16(0.5f)));		// gradient in eye-white
    col.w = S(F16(0.5), F16(0.48), d);									// eye mask
    
    apply_xyz(&col, vec3_mul_one(get_xyz(col), fix16_sub(F16(1.0),fix16_mul(fix16_mul(S(F16(0.45), F16(0.5), d),F16(0.5)),sat(fix16_sub(fix16_sub(F16(0.0),uv.y),fix16_mul(uv.x,side))))))); 	// eye shadow
    
    d = vec2_length(vec2_sub(uv,vec2_mul_one(m,F16(0.4))));									// offset iris pos to look at mouse cursor
    apply_xyz(&col,vec3_mix(get_xyz(col), (vec3){F16(0.0),F16(0.0),F16(0.0)}, S(F16(0.3), F16(0.28), d))); 		// iris outline
    
    apply_xyz(&irisCol,vec3_mul_one(get_xyz(irisCol), fix16_add(F16(1.0), S(F16(0.3), F16(0.05), d))));						// iris lighter in center
    fix16_t irisMask = S(F16(0.28), F16(0.25), d);
    apply_xyz(&col,vec3_mix(get_xyz(col), get_xyz(irisCol), irisMask));			// blend in iris
    
    d = vec2_length(vec2_sub(uv,vec2_mul_one(m,F16(0.45))));									// offset pupil to look at mouse cursor
    
    fix16_t pupilSize = mix(F16(0.4), F16(0.16), smile);
    fix16_t pupilMask = S(pupilSize, fix16_mul(pupilSize,F16(0.85)), d);
    pupilMask = fix16_mul(pupilMask,irisMask);
    apply_xyz(&col,vec3_mix(get_xyz(col), (vec3){F16(0.0), F16(0.0), F16(0.0)}, pupilMask));		// blend in pupil
    
    vec2 offs = {fix16_sin(fix16_add(eyeTime,fix16_mul(uv.y,F16(25.0)))), fix16_sin(fix16_add(eyeTime,fix16_mul(uv.x,F16(25.0))))};
    offs = vec2_mul_one(offs,fix16_mul(F16(0.01),fix16_sub(F16(1.0),smile)));
    
    uv = vec2_add(uv,offs);
    fix16_t highlight = S(F16(0.1), F16(0.09), vec2_length(vec2_sub(uv,(vec2){F16(-0.15), F16(0.15)})));
    highlight += S(F16(0.07), F16(0.05), vec2_length(vec2_add(uv,(vec2){F16(-0.08), F16(0.08)})));
    apply_xyz(&col,vec3_mix(get_xyz(col), (vec3){F16(1.0), F16(1.0), F16(1.0)}, highlight));			// blend in highlight
    
    return col;
}

static inline vec4 Mouth(vec2 uv, fix16_t smile) {
    uv = vec2_sub_one(uv,F16(0.5));
	vec4 col = {F16(0.5), F16(0.18), F16(0.05), F16(1.0)};
    
    uv.y = fix16_mul(uv.y,F16(1.5));
    uv.y = fix16_sub(uv.y,fix16_mul(fix16_mul(uv.x,uv.x),fix16_mul(2.0f,smile)));
    
    uv.x = fix16_mul(uv.x,mix(F16(2.5), F16(1.0), smile));
    
    fix16_t d = vec2_length(uv);
    col.w = S(F16(0.5), F16(0.48), d);
    
    vec2 tUv = uv;
    tUv.y = fix16_add(tUv.y,fix16_mul(fix16_add(fix16_mul(fix16_abs(uv.x),F16(0.5)),F16(0.1)),fix16_sub(F16(1.0),smile)));
    fix16_t td = vec2_length(vec2_sub(tUv,(vec2){F16(0.0), F16(0.6)}));
    
    vec3 toothCol = vec3_mul_one((vec3){F16(1.0), F16(1.0), F16(1.0)},S(F16(0.6), F16(0.35), d));
    apply_xyz(&col,vec3_mix(get_xyz(col), toothCol, S(F16(0.4), F16(0.37), td)));
    
    td = vec2_length(vec2_add(uv,(vec2){F16(0.0), F16(0.5)}));
    apply_xyz(&col,vec3_mix(get_xyz(col), (vec3){F16(1.0), F16(0.5), F16(0.5)}, S(F16(0.5), F16(0.2), td)));
    return col;
}

static inline vec4 Head(vec2 uv) {
	vec4 col = {F16(0.9), F16(0.65), F16(0.1), F16(1.0)};
    
    fix16_t d = vec2_length(uv);
    
    col.w = S(F16(0.5), F16(0.49), d);
    
    fix16_t edgeShade = remap01(F16(0.35), F16(0.5), d);
    edgeShade = fix16_mul(edgeShade,edgeShade);
    apply_xyz(&col,vec3_mul_one(get_xyz(col),fix16_sub(F16(1.0),fix16_mul(edgeShade,F16(0.5)))));
    
    apply_xyz(&col,vec3_mix(get_xyz(col), (vec3){F16(0.6), F16(0.3), F16(0.1)}, S(F16(0.47), F16(0.48), d)));
    
    fix16_t highlight = S(F16(0.41), F16(0.405), d);
    highlight = fix16_mul(highlight,remap(F16(0.41), F16(-0.1), F16(0.75), F16(0.0), uv.y));
    highlight = fix16_mul(highlight,S(F16(0.18), F16(0.19), vec2_length(vec2_sub(uv,(vec2){F16(0.21), F16(0.08)}))));
    apply_xyz(&col,vec3_mix(get_xyz(col), (vec3){F16(1.0), F16(1.0), F16(1.0)}, highlight));
    
    d = vec2_length(vec2_sub(uv,(vec2){F16(0.25), F16(-0.2)}));
    fix16_t cheek = fix16_mul(S(F16(0.2),F16(0.01), d),F16(0.4));
    cheek = fix16_mul(cheek,S(F16(0.17), F16(0.16), d));
    apply_xyz(&col,vec3_mix(get_xyz(col), (vec3){F16(1.0), F16(0.1), F16(0.1)}, cheek));
    
    return col;
}

static inline vec4 Smiley(vec2 uv, vec2 m, fix16_t smile, fix16_t eyeTime) {
	vec4 col = {F16(0.0),F16(0.0),F16(0.0),F16(0.0)};
    
    if(vec2_lengthsq(uv) < F16(0.25)) // only bother about pixels that are actually inside the head
    {
        fix16_t side = sign(uv.x);
        uv.x = fix16_abs(uv.x);
        vec4 head = Head(uv);
        col = vec4_mix(col, head, head.w);

        if(vec2_lengthsq(vec2_sub(uv,(vec2){F16(0.2), F16(0.075)})) < F16(0.030625)) {
            vec4 eye = Eye(within(uv, (vec4){F16(0.03), F16(-0.1), F16(0.37), F16(0.25)}), side, m, smile, eyeTime);
            col = vec4_mix(col, eye, eye.w);
        }
        else if(vec2_lengthsq(vec2_sub(uv,(vec2){F16(0.0), F16(-0.15)})) < F16(0.09)) {
            vec4 mouth = Mouth(within(uv, (vec4){F16(-0.3), F16(-0.43), F16(0.3), F16(-0.13)}), smile);
            col = vec4_mix(col, mouth, mouth.w);
        }
        else if(vec2_lengthsq(vec2_sub(uv,(vec2){F16(0.185), F16(0.325)})) < F16(0.0324)) {
            vec4 brow = Brow(within(uv, (vec4){F16(0.03), F16(0.2), F16(0.4), F16(0.45)}), smile);
            col = vec4_mix(col, brow, brow.w);
        }
    }
    
    return col;
}

vec4 mainImage( vec2 uv, vec2 m, fix16_t smile, fix16_t eyeTime )
{
    fix16_t d = vec2_dot(uv, uv);
    uv = vec2_sub(uv, vec2_mul_one(m,sat(fix16_sub(F16(0.23),d))));
    
    vec4 fragColor = Smiley(uv, m, smile, eyeTime);
    return fragColor;
}