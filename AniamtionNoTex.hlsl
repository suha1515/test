cbuffer CBuf
{
	 matrix modelView;
    matrix modelViewProj;
};
cbuffer Cbuf2 :register(b1)
{
	matrix gBones[128];
}
struct VSOut
{
	float3 viewPos : Position;
	float3 viewNormal : Normal;
	float4 pos : SV_POSITION;
};

VSOut main(float3 pos : POSITION, float3 n : Normal, int4 id : BoneID, float4 weight : Weight)
{
	float finalWeight = 1 - (weight[0] + weight[1] + weight[2]);


	float4x4 boneTransform = gBones[id[0]] * weight[0];
	/*boneTransform += gBones[id[1]] * weight[1];
	boneTransform += gBones[id[2]] * weight[2];
	boneTransform += gBones[id[3]] * finalWeight;*/

	VSOut vso;
	//float4 weights = weight;
	//weights.w = 1.0f - dot(weights.xyz, float3(1, 1, 1));
	//float4 localPos = float4(pos, 1.0f);
	//float3 objPos =
	//		  mul(localPos, gBones[id.x] * weights.x);
	//objPos += mul(localPos, gBones[id.y] * weights.y);
	//objPos += mul(localPos, gBones[id.z] * weights.z);
	//objPos += mul(localPos, gBones[id.w] * weights.w);

	//vso.viewPos = (float3)mul(float4(pos, 1.0f), modelView);
	//vso.viewNormal = mul(n, (float3x3) modelView);

	//vso.pos = mul(float4(pos, 1.0f), modelViewProj);

	float3 normal = mul(float4(n,1.0f),(float3x3)boneTransform);
	float4 PosL = mul(float4(pos, 1.0f), boneTransform);
	vso.viewPos = mul(float4(pos,1.f), modelView).xyz;
	vso.viewNormal = mul(n, (float3x3) modelView);
	vso.pos = mul(PosL, modelViewProj);
	//vso.pos = mul(float4(pos,1.0f), modelViewProj);


	return vso;
}