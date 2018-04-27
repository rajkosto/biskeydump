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

inline vec2 within(vec2 uv, vec4 rect) 
{
    vec2 op1 = vec2_sub(&uv,get_xy(&rect));
    vec2 op2 = vec2_sub(get_zw(&rect),get_xy(&rect));
	return vec2_div(&op1,&op2);
}

vec4 Brow(vec2 uv, float smile) {
    float offs = mix(0.2f, 0.0f, smile);
    uv.y += offs;
    
    float y = uv.y;
    uv.y += uv.x*mix(0.5f, 0.8f, smile)-mix(0.1f, 0.3f, smile);
    uv.x -= mix(0.0f, 0.1f, smile);
    uv = vec2_sub_one(&uv, 0.5f);
    
    vec4 col = {0.0f, 0.0f, 0.0f, 0.0f};
    
    float blur = 0.1f;
    
   	float d1 = vec2_length(&uv);
    float s1 = S(0.45f, 0.45f-blur, d1);
    const vec2 const1 = {0.1f, -0.2f};
    const vec2 scaledConst1 = vec2_mul_one(&const1,0.7f);
    vec2 uvSubbedScaled = vec2_sub(&uv,&scaledConst1);
    float d2 = vec2_length(&uvSubbedScaled);
    float s2 = S(0.5f, 0.5f-blur, d2);
    
    float browMask = sat(s1-s2);
    
    float colMask = remap01(0.7f, 0.8f, y)*0.75f;
    colMask *= S(.6, .9, browMask);
    colMask *= smile;
    const vec4 mixConst1 = {0.4f, 0.2f, 0.2f, 1.0f};
    const vec4 mixConst2 = {1.0f, 0.75f, 0.5f, 1.0f};
    vec4 browCol = vec4_mix(&mixConst1, &mixConst2, colMask); 
   
    uv.y += 0.15f-offs*0.5f;
    blur += mix(0.0f, 0.1f, smile);
    d1 = vec2_length(&uv);
    s1 = S(0.45f, 0.45f-blur, d1);
    uvSubbedScaled = vec2_sub(&uv,&scaledConst1);
    d2 = vec2_length(&uvSubbedScaled);
    s2 = S(0.5f, 0.5f-blur, d2);
    float shadowMask = sat(s1-s2);
    
    const vec4 zeroRGBoneA = {0.0f,0.0f,0.0f,1.0f};
    col = vec4_mix(&col, &zeroRGBoneA, S(0.0f, 1.0f, shadowMask)*0.5f);    
    col = vec4_mix(&col, &browCol, S(0.2f, 0.4f, browMask));
    
    return col;
}

vec4 Eye(vec2 uv, float side, vec2 m, float smile) {
    uv = vec2_sub_one(&uv,0.5f);
    uv.x *= side;
    
	float d = vec2_length(&uv);
    const vec4 allOnes = {1.0f, 1.0f, 1.0f, 1.0f};
    vec4 irisCol = {0.3f, 0.5f, 1.0f, 1.0f};
    vec4 col = vec4_mix(&allOnes, &irisCol, S(0.1f, 0.7f, d)*0.5f);		// gradient in eye-white
    col.w = S(0.5f, 0.48f, d);									// eye mask
    
    *(vec3*)(&col) = vec3_mul_one(get_xyz(&col), 1.0f - S(0.45f, 0.5f, d)*0.5f*sat(-uv.y-uv.x*side)); 	// eye shadow
    
    vec2 mMuled1 = vec2_mul_one(&m,0.4f);
    vec2 subMuled1 = vec2_sub(&uv,&mMuled1);
    d = vec2_length(&subMuled1);									// offset iris pos to look at mouse cursor
    const vec3 threeZeroes = {0.0f,0.0f,0.0f};
    *(vec3*)(&col) = vec3_mix(get_xyz(&col), &threeZeroes, S(0.3f, 0.28f, d)); 		// iris outline
    
    *(vec3*)(&irisCol) = vec3_mul_one(get_xyz(&irisCol), 1.0f + S(0.3f, 0.05f, d));						// iris lighter in center
    float irisMask = S(0.28f, 0.25f, d);
    *(vec3*)(&col) = vec3_mix(get_xyz(&col), get_xyz(&irisCol), irisMask);			// blend in iris
    
    mMuled1 = vec2_mul_one(&m,0.45f);
    subMuled1 = vec2_sub(&uv,&mMuled1); 
    d = vec2_length(&subMuled1);									// offset pupil to look at mouse cursor
    
    float pupilSize = mix(0.4f, 0.16f, smile);
    float pupilMask = S(pupilSize, pupilSize*0.85f, d);
    pupilMask *= irisMask;
    *(vec3*)(&col) = vec3_mix(get_xyz(&col), &threeZeroes, pupilMask);		// blend in pupil
    
    float t = g_iTime*3.0f;
    vec2 offs = {sinf(t+uv.y*25.0f), sinf(t+uv.x*25.0f)};
    offs = vec2_mul_one(&offs,0.01f*(1.0f-smile));
    
    uv = vec2_add(&uv,&offs);
    const vec2 zeroFifteen = {-0.15f, 0.15f};
    vec2 uvToComputeLenOf = vec2_sub(&uv,&zeroFifteen);
    float highlight = S(0.1f, 0.09f, vec2_length(&uvToComputeLenOf));
    const vec2 zeroZeroEight = {-0.08f, 0.08f};
    uvToComputeLenOf = vec2_add(&uv,&zeroZeroEight);
    highlight += S(0.07f, 0.05f, vec2_length(&uvToComputeLenOf));
    *(vec3*)(&col) = vec3_mix(get_xyz(&col), get_xyz(&allOnes), highlight);			// blend in highlight
    
    return col;
}

vec4 Mouth(vec2 uv, float smile) {
    uv = vec2_sub_one(&uv,0.5f);
	vec4 col = {0.5f, 0.18f, 0.05f, 1.0f};
    
    uv.y *= 1.5f;
    uv.y -= uv.x*uv.x*2.0f*smile;
    
    uv.x *= mix(2.5f, 1.0f, smile);
    
    float d = vec2_length(&uv);
    col.w = S(0.5f, 0.48f, d);
    
    vec2 tUv = uv;
    tUv.y += (fabs(uv.x)*0.5f+0.1f)*(1.0f-smile);
    const vec2 toSubConst1 = {0.0f, 0.6f};
    vec2 toCalcLenOf = vec2_sub(&tUv,&toSubConst1);
    float td = vec2_length(&toCalcLenOf);

    const vec3 allOnes = {1.0f, 1.0f, 1.0f};
    vec3 toothCol = vec3_mul_one(&allOnes,S(0.6f, 0.35f, d));
    *(vec3*)(&col) = vec3_mix(get_xyz(&col), &toothCol, S(0.4f, 0.37f, td));
    
    const vec2 toAddConst1 = {0.0f, 0.5f};
    toCalcLenOf = vec2_add(&uv,&toAddConst1);
    td = vec2_length(&toCalcLenOf);
    const vec3 mixConst3 = {1.0f, 0.5f, 0.5f};
    *(vec3*)(&col) = vec3_mix(get_xyz(&col), &mixConst3, S(0.5f, 0.2f, td));
    return col;
}

vec4 Head(vec2 uv) {
	vec4 col = {0.9f, 0.65f, 0.1f, 1.0f};
    
    float d = vec2_length(&uv);
    
    col.w = S(0.5f, 0.49f, d);
    
    float edgeShade = remap01(0.35f, 0.5f, d);
    edgeShade *= edgeShade;
    *(vec3*)(&col) = vec3_mul_one(get_xyz(&col),1.0f-edgeShade*0.5f);
    const vec3 mixConst2 = {0.6f, 0.3f, 0.1f};
    *(vec3*)(&col) = vec3_mix(get_xyz(&col), &mixConst2, S(0.47f, 0.48f, d));
    
    float highlight = S(0.41f, 0.405, d);
    highlight *= remap(0.41f, -0.1f, 0.75f, 0.0f, uv.y);
    const vec2 uvSubConst1 = {0.21f, 0.08f};
    vec2 toCalcLenOf = vec2_sub(&uv,&uvSubConst1);
    highlight *= S(0.18f, 0.19f, vec2_length(&toCalcLenOf));
    const vec3 allOnes = {1.0f, 1.0f, 1.0f};
    *(vec3*)(&col) = vec3_mix(get_xyz(&col), &allOnes, highlight);
    
    const vec2 uvSubConst2 = {0.25f, -0.2f};
    toCalcLenOf = vec2_sub(&uv,&uvSubConst2);
    d = vec2_length(&toCalcLenOf);
    float cheek = S(0.2f,0.01f, d)*0.4f;
    cheek *= S(0.17f, 0.16f, d);
    const vec3 mostlyRed = {1.0f, 0.1f, 0.1f};
    *(vec3*)(&col) = vec3_mix(get_xyz(&col), &mostlyRed, cheek);
    
    return col;
}

vec4 Smiley(vec2 uv, vec2 m, float smile) {
	vec4 col = {0.0f,0.0f,0.0f,0.0f};
    
    if(vec2_length(&uv)<0.5f) // only bother about pixels that are actually inside the head
    {
        float side = sign(uv.x);
        uv.x = fabs(uv.x);
        vec4 head = Head(uv);
        col = vec4_mix(&col, &head, head.w);

        const vec2 eyeUvSubConst = {0.2f, 0.075f};
        const vec2 mouthUvSubConst = {0.0f, -0.15f};
        const vec2 browUvSubConst = {0.185f, 0.325f};

        vec2 toCalcLenOf = vec2_sub(&uv,&eyeUvSubConst);
        if(vec2_length(&toCalcLenOf)<0.175f) {
            vec4 eye = Eye(within(uv, (vec4){0.03f, -0.1f, 0.37f, 0.25f}), side, m, smile);
            col = vec4_mix(&col, &eye, eye.w);
        }

        toCalcLenOf = vec2_sub(&uv,&mouthUvSubConst);
        if(vec2_length(&toCalcLenOf)<0.3f) {
            vec4 mouth = Mouth(within(uv, (vec4){-0.3f, -0.43f, 0.3f, -0.13f}), smile);
            col = vec4_mix(&col, &mouth, mouth.w);
        }

        toCalcLenOf = vec2_sub(&uv,&browUvSubConst);
        if(vec2_length(&toCalcLenOf)<0.18f) {
            vec4 brow = Brow(within(uv, (vec4){0.03f, 0.2f, 0.4f, 0.45f}), smile);
            col = vec4_mix(&col, &brow, brow.w);
        }
    }
    
    return col;
}

vec4 mainImage( vec2 uv )
{
	float t = g_iTime;
    
    /*vec2 uv = vec2_div(fragCoord,iResolution);
    uv = vec2_sub_one(uv,0.5f);
    uv.x *= iResolution.x/iResolution.y;*/
    
    vec2 m; // make it that the eyes look around as time passes
    {
        vec2 rotVect = {sinf(t*0.5f), cosf(t*0.38f)};        
        m = vec2_mul_one(&rotVect,0.4f);
    }
    
    if(vec2_length(&m) > 0.707f) 
        m = (vec2){0.0f,0.0f};	// fix bug with sudden eye move
    
    float d = vec2_dot(&uv, &uv);
    vec2 mulledM = vec2_mul_one(&m,sat(0.23f-d));
    uv = vec2_sub(&uv, &mulledM);
    
    float smile = sinf(t*0.5f)*0.5f+0.5f;
	vec4 fragColor = Smiley(uv, m, smile);
    return fragColor;
}