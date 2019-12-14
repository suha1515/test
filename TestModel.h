#pragma once
#include "AssimpSetting.h"
#include <filesystem>
#include "BindableCommon.h"
#include "Drawable.h"
#include "Bindable.h"
#include "ModelStructure.h"

#define ZERO_MEM(a) memset(a, 0, sizeof(a))
#define ZERO_MEM_VAR(var) memset(&var, 0, sizeof(var))
#define ARRAY_SIZE_IN_ELEMENTS(a) (sizeof(a)/sizeof(a[0]))
class DLL TestModel
{
	struct PSMaterialConstantNotex
	{
		DirectX::XMFLOAT4 materialColor = { 0.447970f,0.327254f,0.176283f,1.0f };
		DirectX::XMFLOAT4 specularColor = { 0.65f,0.65f,0.65f,1.0f };
		float specularPower = 120.0f;
		float padding[3];
	};
	struct Transforms2
	{
		DirectX::XMMATRIX modelView;
		DirectX::XMMATRIX modelViewProj;
	};
	struct CUSTOMVERTEX
	{
		XMFLOAT3 postion;
		XMFLOAT3 normal;
		XMINT4	 boneID;
		XMFLOAT4 weigth;
	};
	struct Vertex
	{
		XMFLOAT3 postion;
		XMFLOAT3 normal;
		XMINT4	 boneID;
		XMFLOAT4 weigth;
	};
	struct VertexBoneData
	{
		int IDs[4];
		float Weights[4];

		VertexBoneData()
		{
			Reset();
		};

		void Reset()
		{
			ZERO_MEM(IDs);
			ZERO_MEM(Weights);
		}

		void AddBoneData(UINT BoneID, float Weight);
	};
	class Dynamic_Mesh : public Drawable
	{

	public:
		Dynamic_Mesh(Graphics& gfx, std::vector<std::shared_ptr<Bind::Bindable>> bindPtrs);
		void Draw(Graphics& gfx, DirectX::FXMMATRIX accumulatedTransform) const noxnd;
		DirectX::XMMATRIX GetTransformXM() const noexcept override;
	private:
		//mutable 키워드는 const 멤버 함수에서도 값이 바뀌는 변수이다.
		mutable DirectX::XMFLOAT4X4 transform;
		MeshEntry	meshInfo;
	};
public:
	TestModel(Graphics& gfx, const std::string& filePath, float scale = 1.0f);
	~TestModel();
	void Update(Graphics& gfx,double timeDelta);
	void Render(Graphics& gfx);
public:
	void MakeModel(Graphics& gfx, const std::string& filePath, float scale);
	void InitMesh(UINT meshIndex,const aiMesh* paiMesh, 
				std::vector<XMFLOAT3>& Positions,
				std::vector<XMFLOAT3>& Normals,
				std::vector<VertexBoneData>& Bones,
				std::vector<UINT>& Indices);
	void LoadBone(UINT meshIndex, const aiMesh* paiMesh, std::vector<VertexBoneData>& Bones);
	void InitMaterial(const std::string& fileName);
public:
	void BoneTransform(double timeInSeconds, std::vector < XMMATRIX> & transforms);
	void ReadNodeHeirarchy(double animationTime, const aiNode* pNode, const XMMATRIX& parentTransform);
	const aiNodeAnim* FindNodeAnim(const aiAnimation* pAnimation, const  std::string& nodeName);

public:
	void CalcInterpolatedScaling(aiVector3D& Out, float AnimationTime, const aiNodeAnim* pNodeAnim);
	void CalcInterpolatedRotation(aiQuaternion& Out, float AnimationTime, const aiNodeAnim* pNodeAnim);
	void CalcInterpolatedPosition(aiVector3D& Out, float AnimationTime, const aiNodeAnim* pNodeAnim);
	UINT FindScaling(float AnimationTime, const aiNodeAnim* pNodeAnim);
	UINT FindRotation(float AnimationTime, const aiNodeAnim* pNodeAnim);
	UINT FindPosition(float AnimationTime, const aiNodeAnim* pNodeAnim);
private:
	// 루트 노드
	std::unique_ptr<NodeInfo> pRoot;
	XMMATRIX			m_GlobalInverseTransform;

	// 본 정보
	std::map<std::string, UINT> m_BoneMapping; // maps a bone name to its index
	UINT m_NumBones;
	std::vector<Bone> m_BoneInfo;


	// 메쉬정보
	std::unique_ptr<Dynamic_Mesh> Meshes;

	// 메쉬 정점,인덱스 정보
	std::vector<MeshEntry> m_Entries;

	const aiScene* m_pScene;
	Assimp::Importer m_Importer;

	// 모든 뼈의 행렬 벡터
	std::vector<XMMATRIX> m_BonesTransMat;

	// 아래 주석 사용안함
	//
	//ID3D11Buffer* mVB;
	//UINT offset;
	//UINT vBstride;
	//// 인덱스 버퍼
	//ID3D11Buffer* mIB;
	//UINT IndexCount;
	//// 버텍스  쉐이더
	//ID3D11VertexShader* pVertexShader;
	//// 픽셀 쉐이더
	//ID3D11PixelShader* pPixelShader;
	//// 인풋 레이아웃
	//ID3D11InputLayout* pInputLayout;
	//// 픽쉘 쉐이더 상수버퍼 들
	//ID3D11Buffer* pixelConstantBuff;
	//// 트랜스폼 상수버퍼
	//ID3D11Buffer* transbuffer;
	//// 본버퍼
	//ID3D11Buffer* boneBuffer;
	//XMMATRIX model;

	
	std::shared_ptr<Bind::VertexBuffer> vertexBuf;
	std::shared_ptr<Bind::IndexBuffer> IndexBuf;
	std::shared_ptr<Bind::VertexShader> vertexShader;
	std::shared_ptr<Bind::PixelShader> PixelShader;
	std::shared_ptr<Bind::InputLayout> InputLayout;
	std::shared_ptr<Bind::PixelConstantBuffer<PSMaterialConstantNotex>> pixelConst;
	std::shared_ptr<Bind::VertexConstantBuffer<BoneBuffer>> bonbufff;
	std::shared_ptr<Bind::VertexConstantBuffer<Transforms2>> Transbufff;
	std::shared_ptr<Bind::Topology> topology;


};