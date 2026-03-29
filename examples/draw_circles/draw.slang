
struct Vertex
{
    float2 position;
    float3 offset_radius;
    float4 color;
}

struct VSOutput {
    float4 screen_position : SV_Position;
    float4 color;
    float3 position;
};

[shader("vertex")]
VSOutput main(in Vertex vertex)
{
    VSOutput output;
    const float2 offset = vertex.offset_radius.xy;
    const float radius = vertex.offset_radius.z;
    output.screen_position = float4(vertex.position * radius + offset, 0, 1);
    output.color = vertex.color;
    output.position = float3(vertex.position, radius);
    return output;
}

enum Mode {
    Normal         = 0,
	Cutout         = 1,
	Cutout_With_AA = 2,
};

[SpecializationConstant]
const int mode = Mode.Normal;

[shader("fragment")]
float4 main(in VSOutput input)
{
    switch (mode)
    {
	default:
    case Mode.Normal:
    {
        return input.color;
	}
    case Mode.Cutout:
	{
        if (dot(input.position.xy, input.position.xy) > 1)
        {
            discard;
        }
        return input.color;
    }
    case Mode.Cutout_With_AA:
    {
        float r = input.position.z;
        float k = 2; // This is just some arbitrary constant
        float m = k * 2.0f / 512.0f;
        float d = r * dot(input.position.xy, input.position.xy);
        if (d > r + m/2) discard;
        float a = saturate((r + m/2 - d) / m);
        return float4(input.color.rgb, input.color.a * a);
    }
	}
}
