Texture2D<float4> gTex : register(t0, space0);
RWStructuredBuffer<uint4> gOut : register(u1, space0);

[numthreads(1, 1, 1)]
void main() {
    float4 c = gTex.Load(int3(0, 0, 0));
    gOut[0]  = asuint(c);
}
