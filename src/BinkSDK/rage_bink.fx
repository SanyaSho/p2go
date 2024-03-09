#define NO_SKINNING
#include "../../../base/src/shaderlib/rage_common.fxh"
#include "../../../base/src/shaderlib/rage_samplers.fxh"

BeginSampler(sampler2D, yPlane, YPlaneSampler, YPlane)
ContinueSampler(sampler2D, yPlane, YPlaneSampler, YPlane)
	MAGFILTER = LINEAR;
	MINFILTER = LINEAR;
	AddressU = CLAMP;
	AddressV = CLAMP;
	AddressW  = CLAMP;
EndSampler;

BeginSampler(sampler2D, cRPlane, cRPlaneSampler, cRPlane)
ContinueSampler(sampler2D, cRPlane, cRPlaneSampler, cRPlane)
	MagFilter = LINEAR;
	MinFilter = LINEAR;
	AddressU = CLAMP;
	AddressV = CLAMP;
	AddressW = CLAMP;
EndSampler;

BeginSampler(sampler2D, cBPlane, cBPlaneSampler, cBPlane)
ContinueSampler(sampler2D, cBPlane, cBPlaneSampler, cBPlane)
	MagFilter = LINEAR;
	MinFilter = LINEAR;
	AddressU = CLAMP;
	AddressV = CLAMP;
	AddressW = CLAMP;
EndSampler;

BeginSampler(sampler2D, aPlane, APlaneSampler, APlane)
ContinueSampler(sampler2D, aPlane, APlaneSampler, APlane)
	MagFilter = LINEAR;
	MinFilter = LINEAR;
	AddressU = CLAMP;
	AddressV = CLAMP;
	AddressW = CLAMP;
EndSampler;

#if RSG_DURANGO
BeginSampler(sampler2D, rgbColor, rgbColorSampler, RGBColor)
ContinueSampler(sampler2D, rgbColor, rgbColorSampler, RGBColor)
	MAGFILTER = LINEAR;
	MINFILTER = LINEAR;
	AddressU = CLAMP;
	AddressV = CLAMP;
	AddressW  = CLAMP;
EndSampler;
#endif

BeginConstantBufferDX10(rage_bink_locals)
ROW_MAJOR float4x4 YUVtoRGB = 
{
	1.164123535f,	1.595794678f,	0.0f,			-0.87065506f,
	1.164123535f,	-0.813476563f,	-0.391448975f,	0.529705048f,
	1.164123535f,	0.0f,			2.017822266f,	-1.081668854f,
	1.0f,			0.0f,			0.0f,			1.0f
};

float4 UVScalar = float4(1.0f, 1.0f, 1.0f, 1.0f);

EndConstantBufferDX10(rage_bink_locals)

struct vertexInput 
{
    float3 pos				: POSITION;
    float2 uv				: TEXCOORD0;
    float4 diffuse			: COLOR0;
};

struct vertexOutput  
{
   DECLARE_POSITION(pos)
   float4 color0:	COLOR0;
   float4 uv:		TEXCOORD0;
};

vertexOutput VS_Bink( vertexInput IN ) 
{
	vertexOutput OUT;
	OUT.pos = mul(float4(IN.pos,1), gWorldViewProj);
	OUT.uv = float4(IN.uv.xy,IN.uv.xy) * UVScalar;
	OUT.color0 = IN.diffuse;
	
	return OUT;
}

float4 PS_Bink( vertexOutput IN ) : COLOR
{

	float4 c = float4(	tex2D(YPlaneSampler, IN.uv.xy).x,
						tex2D(cRPlaneSampler, IN.uv.zw).x,
						tex2D(cBPlaneSampler, IN.uv.zw).x, 
						YUVtoRGB[3].x);
	float4 p = float4(	dot(YUVtoRGB[0], c),
						dot(YUVtoRGB[1], c),
						dot(YUVtoRGB[2], c),
						tex2D(APlaneSampler, IN.uv.xy).x * YUVtoRGB[3].w );			

	return p * IN.color0;
}

float4 PS_BinkNoAlpha( vertexOutput IN ) : COLOR
{
	float4 c = float4(	tex2D(YPlaneSampler, IN.uv.xy).x,
						tex2D(cRPlaneSampler, IN.uv.zw).x,
						tex2D(cBPlaneSampler, IN.uv.zw).x, 
						YUVtoRGB[3].x);
	float4 p = float4(	dot(YUVtoRGB[0], c),
						dot(YUVtoRGB[1], c),
						dot(YUVtoRGB[2], c),
						YUVtoRGB[3].w );

	return p * IN.color0;
}

#if RSG_DURANGO
half3 rgb2yuv(half3 rgb)
{
	half y = dot(rgb,half3(0.299f,0.587f,0.114f));
    half u = (rgb.z - y) * 0.565f;
    half v = (rgb.x - y) * 0.713f;

	return half3(y, u, v) + half3(0.0, 0.5, 0.5);
}

half4 PS_BinkEncodeLuma(vertexOutput IN) : COLOR
{
	half3 rgb = tex2D(rgbColorSampler, IN.uv.xy);

	half3 yuv = rgb2yuv(rgb);
	return yuv.xxxx;
}
 
uint4 PS_BinkEncodeChroma(vertexOutput IN) : SV_Target0
{
	half3 rgb = tex2D(rgbColorSampler, IN.uv.xy);

	half3 yuv = rgb2yuv(rgb);
	const float sclToUint = 255.0;	
	return uint4((uint)(yuv.y * sclToUint), (uint)(yuv.z * sclToUint), 0, 0);
}
#endif

technique bink_alpha
{
	pass p0 
	{      
		VertexShader = compile VERTEXSHADER VS_Bink();
		PixelShader  = compile PIXELSHADER PS_Bink();
	}
}

technique bink_opaque
{
	pass p0 
	{   
		VertexShader = compile VERTEXSHADER VS_Bink();
		PixelShader  = compile PIXELSHADER PS_BinkNoAlpha();
	}
}

#if RSG_DURANGO
technique bink_encode_luma
{
	pass p0 
	{   
		VertexShader = compile VERTEXSHADER VS_Bink();
		PixelShader  = compile PIXELSHADER PS_BinkEncodeLuma();
	}	
}
technique bink_encode_chroma
{
	pass p0
	{
		VertexShader = compile VERTEXSHADER VS_Bink();
		PixelShader  = compile PIXELSHADER PS_BinkEncodeChroma();
	}
}
#endif
