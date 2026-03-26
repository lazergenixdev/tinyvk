
RWStructuredBuffer<float4> instance_buffer;
RWStructuredBuffer<float2> velocity_buffer;

[numthreads(64, 1, 1)]
[shader("compute")]
void main(uint3 index: SV_DispatchThreadID)
{
    float4 instance = instance_buffer[index.x];
    float2 velocity = velocity_buffer[index.x];
    float2 position = instance.xy;
    float  radius = instance.z;

    position += velocity * 0.01;

    if (position.x + radius > 1.0) {
        position.x += 2.0 * (1.0 - position.x - radius);
        velocity.x = -velocity.x;
    }
    if (position.y + radius > 1.0) {
        position.y += 2.0 * (1.0 - position.y - radius);
        velocity.y = -velocity.y;
    }
    if (position.x - radius < -1.0) {
        position.x += 2.0 * (-1.0 - position.x + radius);
        velocity.x = -velocity.x;
    }
    if (position.y - radius < -1.0) {
        position.y += 2.0 * (-1.0 - position.y + radius);
        velocity.y = -velocity.y;
    }

    instance_buffer[index.x] = float4(position, instance.zw);
    velocity_buffer[index.x] = velocity;
}
