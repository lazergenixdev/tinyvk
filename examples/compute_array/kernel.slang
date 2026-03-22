
RWStructuredBuffer<float> buffer;

[numthreads(64, 1, 1)]
[shader("compute")]
void main(uint3 index: SV_DispatchThreadID)
{
    float value = buffer[index.x];
    buffer[index.x] = value * value;
}
