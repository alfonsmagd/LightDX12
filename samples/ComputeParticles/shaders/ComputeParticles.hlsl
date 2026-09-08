struct Particle
{
    float3 position;
    float restRadius;
    float3 velocity;
    float padding;
};

cbuffer PushConstants : register(b0)
{
    float4x4 viewProjection;
    uint particleSrvIndex;
    uint particleUavIndex;
    uint particleCount;
    float deltaTime;
    float impulse;
};

[numthreads(256, 1, 1)]
void CSMain(uint3 threadId : SV_DispatchThreadID)
{
    if (threadId.x >= particleCount)
    {
        return;
    }

    RWStructuredBuffer<Particle> particles = ResourceDescriptorHeap[particleUavIndex];
    Particle particle = particles[threadId.x];

    const float distanceFromCenter = max(length(particle.position), 0.0001);
    const float3 direction = particle.position / distanceFromCenter;
    const float3 restingPosition = direction * particle.restRadius;

    particle.velocity += (restingPosition - particle.position) * 18.0 * deltaTime;
    particle.velocity += direction * impulse;
    particle.velocity *= exp(-3.0 * deltaTime);
    particle.position += particle.velocity * deltaTime;

    particles[threadId.x] = particle;
}

struct VertexOutput
{
    float4 position : SV_Position;
    float3 color : COLOR0;
};

VertexOutput VSMain(uint vertexId : SV_VertexID)
{
    StructuredBuffer<Particle> particles = ResourceDescriptorHeap[particleSrvIndex];
    const Particle particle = particles[vertexId];

    VertexOutput output;
    const float3 normal = normalize(particle.position);
    const float light = 0.25 + 0.75 * saturate(dot(normal, normalize(float3(-0.4, 0.7, -1.0))));
    output.position = mul(viewProjection, float4(particle.position, 1.0));
    output.color = float3(particle.position.xyz) * light;
    return output;
}

float4 PSMain(VertexOutput input) : SV_Target0
{
    return float4(input.color, 1.0);
}
