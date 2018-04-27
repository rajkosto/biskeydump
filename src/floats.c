#include "floats.h"

#define SQRT_MAGIC_F 0x5f3759df 
float sqrtf(const float x)
{
  const float xhalf = 0.5f*x;
 
  union // get bits for floating value
  {
    float x;
    int i;
  } u;
  u.x = x;
  u.i = SQRT_MAGIC_F - (u.i >> 1);  // gives initial guess y0
  return x*u.x*(1.5f - xhalf*u.x*u.x);// Newton step, repeating increases accuracy 
}

float floatFloor(float x) { return (x > 0) ? (int)x : (int)x - 1; }

float floatMod(float a, float b)
{
    return (a - b * floatFloor(a / b));
}

#define MY_PI (3.14159265f)
#define MY_2PI (6.28318531f)
float normalizeAngle(float angle)
{
    double a = floatMod(angle + MY_PI, MY_2PI);
    return a >= 0 ? (a - MY_PI) : (a + MY_PI);
}

float sinf(float x)
{
    //always wrap input angle to -PI..PI
    /*while (x < -MY_PI)
        x += MY_2PI;
    
    while (x > MY_PI)
        x -= MY_2PI;*/

    x = normalizeAngle(x);

    //compute sine
    if (x < 0)
        return 1.27323954f * x + 0.405284735f * x * x;
    else
        return 1.27323954f * x - 0.405284735f * x * x;
}

float cosf(float x)
{
    //compute cosine: sin(x + PI/2) = cos(x)
    x += 1.57079632f;
    /*while (x < -MY_PI)
        x += MY_2PI;
    
    while (x > MY_PI)
        x -= MY_2PI;*/

    x = normalizeAngle(x);

    if (x < 0)
        return 1.27323954f * x + 0.405284735f * x * x;
    else
        return 1.27323954f * x - 0.405284735f * x * x;
}