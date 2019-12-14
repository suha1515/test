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
		//mutable Ű����� const ��� �Լ������� ���� �ٲ�� �����̴�.
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
	// ��Ʈ ���
	std::unique_ptr<NodeInfo> pRoot;
	XMMATRIX			m_GlobalInverseTransform;

	// �� ����
	std::map<std::string, UINT> m_BoneMapping; // maps a bone name to its index
	UINT m_NumBones;
	std::vector<Bone> m_BoneInfo;


	// �޽�����
	std::unique_ptr<Dynamic_Mesh> Meshes;

	// �޽� ����,�ε��� ����
	std::vector<MeshEntry> m_Entries;

	const aiScene* m_pScene;
	Assimp::Importer m_Importer;

	// ��� ���� ��� ����
	std::vector<XMMATRIX> m_BonesTransMat;

	// �Ʒ� �ּ� ������
	//
	//ID3D11Buffer* mVB;
	//UINT offset;
	//UINT vBstride;
	//// �ε��� ����
	//ID3D11Buffer* mIB;
	//UINT IndexCount;
	//// ���ؽ�  ���̴�
	//ID3D11VertexShader* pVertexShader;
	//// �ȼ� ���̴�
	//ID3D11PixelShader* pPixelShader;
	//// ��ǲ ���̾ƿ�
	//ID3D11InputLayout* pInputLayout;
	//// �Ƚ� ���̴� ������� ��
	//ID3D11Buffer* pixelConstantBuff;
	//// Ʈ������ �������
	//ID3D11Buffer* transbuffer;
	//// ������
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