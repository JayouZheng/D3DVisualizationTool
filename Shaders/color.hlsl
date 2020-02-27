//
// color.hlsl
// #pragma pack_matrix( row_major ) column_major default.

struct PerBoxData
{
    float4 Color;
};

StructuredBuffer<PerBoxData> gPerBoxData : register(t0);

cbuffer PerPassCBuffer : register(b0)
{
    float4x4 gViewProj;
};

cbuffer PerObjectCBuffer : register(b1)
{
    float4x4 gWorld;
    float gOverflow;
    int gPerBoxSBufferOffset;
};

struct VertexIn
{
	float4 PosL  : POSITION;
    float4 Color : COLOR;
};

struct VertexOut
{
	float4 PosH  : SV_POSITION;
    float4 PosW  : POSITION;
    float4 Color : COLOR;
};

VertexOut LineVS(VertexIn vin, uint vertexID : SV_VertexID)
{
	VertexOut vout;
    
    vout.PosW = mul(gWorld, vin.PosL);
	
	// Transform to homogeneous clip space.
    vout.PosH = mul(gViewProj, vout.PosW);
	
	// Just pass vertex color into the pixel shader.
    vout.Color = vin.Color;
    
    return vout;
}

VertexOut VS(VertexIn vin, uint vertexID : SV_VertexID)
{
    VertexOut vout;
    
    vout.PosW = mul(gWorld, vin.PosL);
	
	// Transform to homogeneous clip space.
    vout.PosH = mul(gViewProj, vout.PosW);
	
	// Just pass vertex color into the pixel shader.
    vout.Color = gPerBoxData[gPerBoxSBufferOffset + vertexID/8].Color / gOverflow;
    
    return vout;
}

float4 PS(VertexOut pin) : SV_Target
{
    return pin.Color;
}


