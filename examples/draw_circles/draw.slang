
struct Vertex
{
    float2 position;
    float3 offset_radius;
    float4 color;
}

struct VSOutput {
    float4 screen_position : SV_Position;
    float4 color;
    float2 position;
};

[shader("vertex")]
VSOutput main(in Vertex vertex)
{
    VSOutput output;
    const float2 offset = vertex.offset_radius.xy;
    const float radius = vertex.offset_radius.z;
    output.screen_position = float4(vertex.position * radius + offset, 0, 1);
    output.color = vertex.color;
    output.position = vertex.position;
    return output;
}

[shader("fragment")]
float4 main(in VSOutput input)
{
    if (dot(input.position, input.position) > 1)
    {
        discard;
		return 0;
	}
    return input.color;
}
