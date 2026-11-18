#pragma once

// Feed the stock shader unit-triangle positions (0,0), (1,0), (0,1).
// Keeping input XY in 0..1 also supports TextureMapper variants that clamp
// local coordinates before sampling. Screen placement belongs in the model
// matrix; UVs belong in the texture matrix, as in the OpenOrbis sample.
template<class Vertex>
inline bool triangleLocalMatrices(const Vertex *v,float *model,float *texture)
{
    const float ax=v[1].x-v[0].x, ay=v[1].y-v[0].y;
    const float bx=v[2].x-v[0].x, by=v[2].y-v[0].y;
    const float det=ax*by-ay*bx;
    if(det>-0.000001f && det<0.000001f) return false;
    for(int i=0;i<16;++i) model[i]=texture[i]=0;
    model[0]=ax; model[1]=ay; model[4]=bx; model[5]=by;
    model[12]=v[0].x; model[13]=v[0].y;
    model[10]=model[15]=1;
    texture[0]=v[1].u-v[0].u; texture[1]=v[1].v-v[0].v;
    texture[4]=v[2].u-v[0].u; texture[5]=v[2].v-v[0].v;
    texture[12]=v[0].u; texture[13]=v[0].v;
    texture[10]=texture[15]=1;
    return true;
}
