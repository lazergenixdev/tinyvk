
[format("rgba8")] RWTexture2D<float4> image;

[numthreads(8, 8, 1)]
[shader("compute")]
void main(uint3 index: SV_DispatchThreadID)
{
    uint2 size;
    image.GetDimensions(size.x, size.y);
    float4 value = 1;
    value.xy = float2(index.xy) / float2(size);
    image.Store(index.xy, value);
}
