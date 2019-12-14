#include "TestModel.h"
#include "..\Headers\TestModel.h"
#include <filesystem>
void TestModel::VertexBoneData::AddBoneData(UINT BoneID, float Weight)
{
	for (UINT i = 0; i < ARRAY_SIZE_IN_ELEMENTS(IDs); i++) {
		if (Weights[i] == 0.0) {
			IDs[i] = BoneID;
			Weights[i] = Weight;
			return;
		}
	}

	// should never get here - more bones than we have space for
	assert(0);
}

TestModel::TestModel(Graphics& gfx, const std::string& filePath, float scale)
	:m_NumBones(0),m_GlobalInverseTransform(XMMatrixIdentity()), vBstride(0),IndexCount(0),offset(0)
{
	unsigned int flag = aiProcess_ConvertToLeftHanded	//�޼���ǥ���
		| aiProcess_GenNormals							//��ְ� ����
		| aiProcess_CalcTangentSpace					//ź��Ʈ ���
		| aiProcess_Triangulate							//�ﰢ�����θ����
		| aiProcess_JoinIdenticalVertices;				//����ȭ

	m_pScene = m_Importer.ReadFile(filePath, flag);

	if (m_pScene)
	{
		m_GlobalInverseTransform = XMMatrixTranspose((XMMATRIX)m_pScene->mRootNode->mTransformation[0]);
		XMMatrixInverse(nullptr, m_GlobalInverseTransform);
		MakeModel(gfx, filePath, scale);	//���� ����°�
	}

}

TestModel::~TestModel()
{

}

void TestModel::Update(Graphics& gfx,double timeDelta)
{
	// ��� ��ȸ�ϸ鼭 ��� ������Ʈ
	BoneTransform(timeDelta, m_BonesTransMat);
	// Ʈ������ ������Ʈ
	// �ӽ÷� ���� �������̴� 0�� ������ۿ� ������ ��ĵ� (�𵨺�,�𵨺�����)
	TestModel::Transforms2 tf;
	XMMATRIX modelView = XMMatrixIdentity() * gfx.GetCamera();
	tf.modelView = XMMatrixTranspose(modelView);
	tf.modelViewProj = XMMatrixTranspose(modelView * gfx.GetProjection());

	Transbufff->Update(gfx, tf);

	//������ ���� m_boneTransMat ���Ϳ� ��� �� ��ĵ��� ���̴��� ����
	D3D11_MAPPED_SUBRESOURCE msr;
	gfx.GetContext()->Map(
		boneBuffer, 0u,
		D3D11_MAP_WRITE_DISCARD, 0u,
		&msr);
	memcpy(msr.pData,m_BonesTransMat.data(), sizeof(DirectX::XMMATRIX) * m_BonesTransMat.size());
	gfx.GetContext()->Unmap(boneBuffer, 0u);

	// �������̴� 1�����Կ� ����
	gfx.GetContext()->VSSetConstantBuffers(1, 1u, &boneBuffer);

}

void TestModel::Render(Graphics& gfx)
{	
	// �������� ������ ���ε���ü�� ���ε� �ǽ�
	topology->Bind(gfx);
	vertexBuf->Bind(gfx);
	IndexBuf->Bind(gfx);
	vertexShader->Bind(gfx);
	PixelShader->Bind(gfx);;
	pixelConst->Bind(gfx);
	InputLayout->Bind(gfx);
	Transbufff->Bind(gfx);
	gfx.DrawIndexed(IndexBuf->GetCount());
}

void TestModel::MakeModel(Graphics& gfx, const std::string& filePath, float scale)
{
	m_Entries.resize(m_pScene->mNumMeshes);

	if (m_pScene == nullptr)
		return;

	std::vector<XMFLOAT3> positions;				//���� ������ ��� �����̳�
	std::vector<XMFLOAT3> Normal;					//��ְ�
	std::vector<VertexBoneData> bones;				//������
	std::vector<Vertex> vertex;				
	std::vector<UINT>	Indices;					//�ε���

	UINT NumVertices = 0;
	UINT NUMIndices = 0;

	// ������ �ε��� ���� ����
	for (UINT i = 0; i < m_Entries.size(); ++i)
	{
		m_Entries[i].materialIndex = m_pScene->mMeshes[i]->mMaterialIndex;		//�ش� �޽��� ���� �ε���
		m_Entries[i].numIndices	   = m_pScene->mMeshes[i]->mNumFaces * 3;		//�ش� �޽��� �ε��� ����
		m_Entries[i].baseVertex	   = NumVertices;								//�ش� �޽��� ���� ����
		m_Entries[i].baseIndex	   = NUMIndices;								//�ش� �޽��� ���� �ε���

		NumVertices += m_pScene->mMeshes[i]->mNumVertices;						//�ش�޽� ���� ���� (���� �޽��� ��������)
		NUMIndices  += m_Entries[i].numIndices;									//�ش�޽� �ε��� ���� (���� �޽��� ��������)
	}

	// �޽��� ������ �����ϱ����� �̸� ������ �Ҵ� (����,�ε���,���,���������..)
	positions.reserve(NumVertices);
	Normal.reserve(NumVertices);
	bones.resize(NumVertices);
	vertex.resize(NumVertices);
	Indices.reserve(NUMIndices);

	// �޽��� �Ѱ��� �ʱ�ȭ�Ѵ�.
	for (UINT i = 0; i < m_Entries.size(); ++i)
	{
		const aiMesh* paiMesh = m_pScene->mMeshes[i];
		InitMesh(i, paiMesh, positions, Normal, bones, Indices);	//�޽��ʱ�ȭ
	}

	// �ʱ�ȭ�� ������ �� �����̳ʿ��� ���� ������ ����.
	// �� ����������ŭ �������� ���� ���̴� ������ ������ ��ġ,���,�� ID,����ġ�̴�.
	// ���� ���� ����ġ�� XMINT4,XMFLOAT4
	for (UINT i = 0; i < NumVertices; ++i)
	{
		XMINT4 boneID = XMINT4(bones[i].IDs);
		XMFLOAT4 weight = XMFLOAT4(bones[i].Weights);
		vertex[i] = { positions[i],Normal[i],boneID,weight };
	}
	
	// ���������� ���� ����
	Dvtx::VertexBuffer vbuf(std::move(Dvtx::VertexLayout{})
		.Append(Dvtx::VertexLayout::Position3D).Append(Dvtx::VertexLayout::Normal)
		.Append(Dvtx::VertexLayout::BoneID).Append(Dvtx::VertexLayout::Weight));
	
	// �� ����������ŭ �ݸ������� ���ۿ� ������ ä���.
	for (UINT i = 0; i < NumVertices; ++i)
	{
		vbuf.EmplaceBack(
			XMFLOAT3(positions[i].x * scale, positions[i].y * scale, positions[i].z * scale),				//��ġ
			XMFLOAT3(Normal[i].x, Normal[i].y , Normal[i].z ),												//���
			XMINT4(bones[i].IDs[0], bones[i].IDs[1], bones[i].IDs[2], bones[i].IDs[3]),						//��ID
			XMFLOAT4(bones[i].Weights[0], bones[i].Weights[1], bones[i].Weights[2], bones[i].Weights[3]));	//����ġ
	}

	//���� �۾��� �Ϲ����� ���������� ���ε� �۾�
	// ��������
	topology = (Bind::Topology::Resolve(gfx,D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST));
	// �������۸� ���۳������� ����
	vertexBuf = (Bind::VertexBuffer::Resolve(gfx, "test", vbuf));
	// �ε��� ���� ����
	IndexBuf = (Bind::IndexBuffer::Resolve(gfx, "test2", Indices));
	// ���� ���̴� ����
	vertexShader = (Bind::VertexShader::Resolve(gfx, "AniamtionNoTex.cso"));
	// �ȼ� ���̴� ����
	PixelShader = (Bind::PixelShader::Resolve(gfx, "AnimationPSNoTex.cso"));
	// �Է� ���̾ƿ� ����
	InputLayout = (Bind::InputLayout::Resolve(gfx, vbuf.GetLayout(), vertexShader->GetBytecode()));


	//  �� ��� ����� ���� ������� ����
	D3D11_BUFFER_DESC cbd;
	cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	cbd.Usage = D3D11_USAGE_DYNAMIC;
	cbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	cbd.MiscFlags = 0u;
	cbd.ByteWidth = sizeof(DirectX::XMMATRIX) * 128;			//�� 128���� 4x4 ����� �����Ѵ�
	cbd.StructureByteStride = 0u;

	gfx.GetDevice()->CreateBuffer(&cbd, nullptr, &boneBuffer);	// �������� ������� ����

	// �߰����� ������ �������� ������ ��������
	PSMaterialConstantNotex psmc;
	psmc.specularPower = 2.0f;
	psmc.specularColor = { 0.18f,0.18f,0.18f,1.0f };
	psmc.materialColor = { 0.45f,0.45f,0.85f,1.0f };
	// �ȼ����̴� ������� 0�������� �̹� ���� ���������� ���������Ƿ�(�ٸ��ڵ忡��)
	// ����ִ� 1�����Կ� �Ҵ�
	pixelConst = (Bind::PixelConstantBuffer<TestModel::PSMaterialConstantNotex>::Resolve(gfx, psmc, 1u));
	// ��ġ�� ���� Ʈ������ ����
	Transbufff = (Bind::VertexConstantBuffer<TestModel::Transforms2>::Resolve(gfx, 0u));
	// ������
	bonbufff = (Bind::VertexConstantBuffer<BoneBuffer>::Resolve(gfx,1u));
}

void TestModel::InitMesh(UINT meshIndex, const aiMesh* paiMesh, std::vector<XMFLOAT3>& Positions, 
						std::vector<XMFLOAT3>& Normals, 
						std::vector<TestModel::VertexBoneData>& Bones,
						std::vector<UINT>& Indices)
{
	const aiVector3D Zero3D(0.0f, 0.0f, 0.0f);

	// �޽��� ������ �����Ѵ�.
	for (UINT i = 0; i < paiMesh->mNumVertices; ++i)
	{
		const aiVector3D* pPos = &(paiMesh->mVertices[i]);
		const aiVector3D* pNormal = &(paiMesh->mNormals[i]);

		Positions.push_back(XMFLOAT3(pPos->x, pPos->y, pPos->z));
		Normals.push_back(XMFLOAT3(pNormal->x, pNormal->y, pNormal->z));
	}

	// �������� �ε��Ѵ�.
	LoadBone(meshIndex, paiMesh, Bones);

	// �ε��� ���۸� �����
	for (UINT i = 0; i < paiMesh->mNumFaces; ++i)
	{
		const aiFace& face = paiMesh->mFaces[i];
		assert(face.mNumIndices == 3);
		Indices.push_back(face.mIndices[0]);
		Indices.push_back(face.mIndices[1]);
		Indices.push_back(face.mIndices[2]);
	}
}

void TestModel::LoadBone(UINT meshIndex, const aiMesh* paiMesh, std::vector<VertexBoneData>& Bones)
{
	// �޽��� ���� ���� ������ ����
	for (UINT i = 0; i < paiMesh->mNumBones; ++i)
	{
		UINT boneIndex = 0;											//���� �ε���
		std::string boneName(paiMesh->mBones[i]->mName.C_Str());	//���� �̸�

		// �ش� ���� �ʿ� �ִ��� �˻�
		if (m_BoneMapping.find(boneName) == m_BoneMapping.end())
		{
			// ������� ���� �� ������ ���ο� �ε������ȴ�.
			boneIndex = m_NumBones;	
			m_NumBones++;
			Bone bi;
			m_BoneInfo.push_back(bi);
			// ���� ����������� �����Ѵ�.
			m_BoneInfo[boneIndex].BoneOffset = XMMatrixTranspose((XMMATRIX)paiMesh->mBones[i]->mOffsetMatrix[0]);
			m_BoneMapping[boneName] = boneIndex;
		}
		else //������ ������� �ε����� ��ȯ
			boneIndex = m_BoneMapping[boneName];

		// �ش���� ����ġ ������ŭ �ݸ��� �����Ѵ�.
		for (UINT j = 0; j < paiMesh->mBones[i]->mNumWeights; ++j)
		{
			// ���� �޽��� ��� �������� ���� ����ġ�� ���� ID ����ü�� ����Ͽ�
			// �ش� �޽��� �����鿡 ��ID�� ����ġ�� �����Ѵ�. 
			UINT vertexID = m_Entries[meshIndex].baseVertex + paiMesh->mBones[i]->mWeights[j].mVertexId;
			float weight = paiMesh->mBones[i]->mWeights[j].mWeight;
			Bones[vertexID].AddBoneData(boneIndex, weight);
		}
	}
}

void TestModel::InitMaterial(const std::string& fileName)
{
	//aiMesh�� ���״�� ���������� �ε����� �̷���� ���ϱ����̴�.
	using namespace std::string_literals;
	using namespace Bind;
	using Dvtx::VertexLayout;
	//�� ���.
	//const auto rootPath = fileName.parent_path().string() + "\\";

	std::vector<std::shared_ptr<Bindable>> bindablePtrs;

	bool hasSpecularMap = false;
	bool hasAlphaGloss = false;
	bool hasNormalMap = false;
	bool hasDiffuseMap = false;
	float shininess = 2.0f;
	XMFLOAT4 specularColor = { 0.18f,0.18f,0.18f,1.0f };
	XMFLOAT4 diffuseColor = { 0.45f,0.45f,0.85f,1.0f };

	//if (mesh.mMaterialIndex >= 0)
	//{
	//	auto& material = *pMaterial[mesh.mMaterialIndex];
	//	aiString texFileName;
	//	material.Get(AI_MATKEY_SHININESS, shininess);
	//	//�ؽ�ó�� ���ٸ� ���͸��󿡼� ������ �����´�.
	//	material.Get(AI_MATKEY_COLOR_DIFFUSE, reinterpret_cast<aiColor3D&>(diffuseColor));
	//	////���͸��󿡼� Diffuse �ؽ�ó ������ �����´�
	//	//if (material.GetTexture(aiTextureType_DIFFUSE, 0, &texFileName) == aiReturn_SUCCESS)
	//	//{
	//	//	//�ش� �ؽ��� Ǫ��
	//	//	bindablePtrs.push_back(Texture::Resolve(gfx, rootPath + texFileName.C_Str()));
	//	//	hasDiffuseMap = true;
	//	//}
	//	//else
	//	//	//�ؽ�ó�� ���ٸ� ���͸��󿡼� ������ �����´�.
	//	//	material.Get(AI_MATKEY_COLOR_DIFFUSE, reinterpret_cast<aiColor3D&>(diffuseColor));

	//	////����ŧ�� �� ������ �����´�
	//	//if (material.GetTexture(aiTextureType_SPECULAR, 0, &texFileName) == aiReturn_SUCCESS)
	//	//{
	//	//	auto tex = Texture::Resolve(gfx, rootPath + texFileName.C_Str(), 1u);
	//	//	hasAlphaGloss = tex->HasAlhpa();
	//	//	bindablePtrs.push_back(std::move(tex));
	//	//	hasSpecularMap = true;
	//	//}
	//	//else
	//	//	//����ŧ�� ���� ���ٸ�.

	//	////������ ����ŧ���ʿ� ���İ��̾������ �⺻ ������
	//	//	if (!hasAlphaGloss)
	//	//		material.Get(AI_MATKEY_SHININESS, shininess);

	//	////��� �� ������ �����´�.
	//	//if (material.GetTexture(aiTextureType_NORMALS, 0, &texFileName) == aiReturn_SUCCESS)
	//	//{
	//	//	auto tex = Texture::Resolve(gfx, rootPath + texFileName.C_Str(), 2u);
	//	//	hasAlphaGloss = tex->HasAlhpa();
	//	//	bindablePtrs.push_back(std::move(tex));
	//	//	hasNormalMap = true;
	//	//}
	//	////3�� �ϳ��� ������ ���÷� �߰�.
	//	//if (hasDiffuseMap || hasSpecularMap || hasNormalMap)
	//	//	bindablePtrs.push_back(Bind::Sampler::Resolve(gfx));
	//}
}

void TestModel::BoneTransform(double timeInSeconds, std::vector<XMMATRIX>& transforms)
{
	XMMATRIX Identity = XMMatrixIdentity();

	// �ִϸ��̼��� ���� ������
	double ticksPerSeconds = m_pScene->mAnimations[0]->mTicksPerSecond != 0 ? m_pScene->mAnimations[0]->mTicksPerSecond : 25.0f;
	double timeInTicks = timeInSeconds * ticksPerSeconds;
	double aniamtionTime = fmod(timeInTicks, m_pScene->mAnimations[0]->mDuration);

	//�� �Լ��� ��͵Ǿ� ������ ��ȸ�ϸ鼭 �������� ������Ʈ�Ѵ�
	ReadNodeHeirarchy(aniamtionTime, m_pScene->mRootNode, Identity);

	//�� ������ŭ Ʈ������ �����̳� ũ������
	transforms.resize(m_NumBones);

	for (UINT i = 0; i < m_NumBones; ++i)
		transforms[i] = XMMatrixTranspose(m_BoneInfo[i].FinalTransformation);
		//transforms[i] = XMMatrixTranspose(XMMatrixTranslation(-7.0f,0.0f,0.0f));
		//transforms[i] = XMMatrixIdentity();


}

void TestModel::ReadNodeHeirarchy(double animationTime, const aiNode* pNode, const XMMATRIX& parentTransform)
{
	
	const std::string& nodeName = pNode->mName.C_Str();  //��� �̸�
	
	// ���� ������ �ִϸ��̼� (ù����)
	const aiAnimation* pAnimation = m_pScene->mAnimations[0];								// ���� ù��° �ִϸ��̼��� �����´�.
	XMMATRIX nodeTransformation = XMMatrixTranspose(XMMATRIX(pNode->mTransformation[0]));	// ����� Ʈ������ ����� �����´�(����)
	const aiNodeAnim* pNodeAnim = FindNodeAnim(pAnimation, nodeName);						// ���� ��尡 �ִϸ��̼��� �ִ��� Ȯ��

	// ���� ��忡 �ִϸ��̼��� �ִٸ�
	if (pNodeAnim)
	{
		//�����ϰ� ����
		pNodeAnim->mScalingKeys[0].mValue;
		aiVector3D  scailing;
		CalcInterpolatedScaling(scailing, animationTime, pNodeAnim);
		//XMMATRIX scalingMat = XMMatrixScaling(scailing.x, scailing.y, scailing.z);
		aiQuaternion rotationQ;
		CalcInterpolatedRotation(rotationQ, animationTime, pNodeAnim);
		//XMMATRIX rotationMat = XMMatrixRotationQuaternion(XMVectorSet(rotationQ.x, rotationQ.y, rotationQ.z, rotationQ.w));
		aiVector3D translation;
		CalcInterpolatedPosition(translation, animationTime, pNodeAnim);
		//XMMATRIX translationMat = XMMatrixTranslation(translation.x, translation.y, translation.z);
		aiMatrix4x4 mat;
		mat = aiMatrix4x4(rotationQ.GetMatrix());
		mat.a1 *= scailing.x; mat.b1 *= scailing.x; mat.c1 *= scailing.x;
		mat.a2 *= scailing.y; mat.b2 *= scailing.y; mat.c2 *= scailing.y;
		mat.a3 *= scailing.z; mat.b3 *= scailing.z; mat.c3 *= scailing.z;
		mat.a4 = translation.x; mat.b4 = translation.y; mat.c4 = translation.z;

		// ������ ������ �����Ͽ� ��ġ ( �ִϸ��̼��� ������� �ش� ���� ���� ����� �ȴ�)
		nodeTransformation = XMMatrixTranspose(XMLoadFloat4x4(
			reinterpret_cast<const XMFLOAT4X4*>(&mat)
		));
	}
	// �θ�κ����� ��İ� �ڽ������ ���� �۷ι� ��� �����
	XMMATRIX GlobalTransformation = nodeTransformation* parentTransform;

	// �ش� ��尡 ����� �ϰ��.
	if (m_BoneMapping.find(nodeName) != m_BoneMapping.end())
	{
		// �ִٸ� ���� �̵���Ű������ ��������ĵ� ���� �������ش�.
		UINT boneIndex = m_BoneMapping[nodeName];
		m_BoneInfo[boneIndex].FinalTransformation = m_BoneInfo[boneIndex].BoneOffset*GlobalTransformation *m_GlobalInverseTransform; //���� �����°��Բ� �����ش�.
		int a = 10;
	}

	// �ڽĶ� �Ȱ��� ��带 ������Ʈ �Ѵ�.
	for (UINT i = 0; i < pNode->mNumChildren; ++i)
		ReadNodeHeirarchy(animationTime, pNode->mChildren[i], GlobalTransformation);
	
}

const aiNodeAnim* TestModel::FindNodeAnim(const aiAnimation* pAnimation, const std::string& nodeName)
{
	// �ش� �ִϸ��̼��� �󸶸�ŭ�� ��带 �����̴°��� mNumChannels �̴�.
	for (UINT i = 0; i < pAnimation->mNumChannels; ++i)
	{
		// �ִϸ��̼��� ��ä�ο� �����Ͽ� �ִϸ��̼� ����� �����͸� �޾ƿ´�
		const aiNodeAnim* pNodeAnim = pAnimation->mChannels[i];

		// �ش� �ִԳ���� �̸��� ������ �̸��� ���ٸ� �ִϸ��̼� ����ΰ��̹Ƿ� ��ȯ.
		if (std::string(pNodeAnim->mNodeName.data) == nodeName)
			return pNodeAnim;
	}
	return nullptr;
}

//������ ����
void TestModel::CalcInterpolatedScaling(aiVector3D& Out, float AnimationTime, const aiNodeAnim* pNodeAnim)
{
	if (pNodeAnim->mNumScalingKeys == 1) {
		Out = pNodeAnim->mScalingKeys[0].mValue;
		return;
	}

	UINT ScalingIndex = FindScaling(AnimationTime, pNodeAnim);
	UINT NextScalingIndex = (ScalingIndex + 1);
	assert(NextScalingIndex < pNodeAnim->mNumScalingKeys);
	float DeltaTime = (float)(pNodeAnim->mScalingKeys[NextScalingIndex].mTime - pNodeAnim->mScalingKeys[ScalingIndex].mTime);
	float Factor = (AnimationTime - (float)pNodeAnim->mScalingKeys[ScalingIndex].mTime) / DeltaTime;
	assert(Factor >= 0.0f && Factor <= 1.0f);
	const aiVector3D& Start = pNodeAnim->mScalingKeys[ScalingIndex].mValue;
	const aiVector3D& End = pNodeAnim->mScalingKeys[NextScalingIndex].mValue;
	aiVector3D Delta = End - Start;
	Out = Start + Factor * Delta;
}
//ȸ�� ����
void TestModel::CalcInterpolatedRotation(aiQuaternion& Out, float AnimationTime, const aiNodeAnim* pNodeAnim)
{
	// we need at least two values to interpolate...
	if (pNodeAnim->mNumRotationKeys == 1) {
		Out = pNodeAnim->mRotationKeys[0].mValue;
		return;
	}

	UINT RotationIndex = FindRotation(AnimationTime, pNodeAnim);
	UINT NextRotationIndex = (RotationIndex + 1);
	assert(NextRotationIndex < pNodeAnim->mNumRotationKeys);
	float DeltaTime = (float)(pNodeAnim->mRotationKeys[NextRotationIndex].mTime - pNodeAnim->mRotationKeys[RotationIndex].mTime);
	float Factor = (AnimationTime - (float)pNodeAnim->mRotationKeys[RotationIndex].mTime) / DeltaTime;
	assert(Factor >= 0.0f && Factor <= 1.0f);
	const aiQuaternion& StartRotationQ = pNodeAnim->mRotationKeys[RotationIndex].mValue;
	const aiQuaternion& EndRotationQ = pNodeAnim->mRotationKeys[NextRotationIndex].mValue;
	aiQuaternion::Interpolate(Out, StartRotationQ, EndRotationQ, Factor);
	Out = Out.Normalize();
}

//�̵�����
void TestModel::CalcInterpolatedPosition(aiVector3D& Out, float AnimationTime, const aiNodeAnim* pNodeAnim)
{
	if (pNodeAnim->mNumPositionKeys == 1) {
		Out = pNodeAnim->mPositionKeys[0].mValue;
		return;
	}

	UINT PositionIndex = FindPosition(AnimationTime, pNodeAnim);
	UINT NextPositionIndex = (PositionIndex + 1);
	assert(NextPositionIndex < pNodeAnim->mNumPositionKeys);
	float DeltaTime = (float)(pNodeAnim->mPositionKeys[NextPositionIndex].mTime - pNodeAnim->mPositionKeys[PositionIndex].mTime);
	float Factor = (AnimationTime - (float)pNodeAnim->mPositionKeys[PositionIndex].mTime) / DeltaTime;
	assert(Factor >= 0.0f && Factor <= 1.0f);
	const aiVector3D& Start = pNodeAnim->mPositionKeys[PositionIndex].mValue;
	const aiVector3D& End = pNodeAnim->mPositionKeys[NextPositionIndex].mValue;
	aiVector3D Delta = End - Start;
	Out = Start + Factor * Delta;
}

// �ش� ����� �ִϸ��̼ǿ��� ũ�⺯ȭ ������ Ű���� �����ϴ��� �˻�
UINT TestModel::FindScaling(float AnimationTime, const aiNodeAnim* pNodeAnim)
{
	assert(pNodeAnim->mNumScalingKeys > 0);

	for (UINT i = 0; i < pNodeAnim->mNumScalingKeys - 1; i++) {
		if (AnimationTime < (float)pNodeAnim->mScalingKeys[i + 1].mTime) {
			return i;
		}
	}
	assert(0);
	return 0;
}
// �ش� ����� �ִϸ��̼ǿ��� ȸ�������� Ű���� �����ϴ��� �˻�
UINT TestModel::FindRotation(float AnimationTime, const aiNodeAnim* pNodeAnim)
{
	assert(pNodeAnim->mNumRotationKeys > 0);

	for (UINT i = 0; i < pNodeAnim->mNumRotationKeys - 1; i++) {
		if (AnimationTime < (float)pNodeAnim->mRotationKeys[i + 1].mTime) {
			return i;
		}
	}
	assert(0);
	return 0;
}
// �ش� ����� �ִϸ��̼ǿ��� �̵������� Ű���� �����ϴ��� �˻�
UINT TestModel::FindPosition(float AnimationTime, const aiNodeAnim* pNodeAnim)
{
	for (UINT i = 0; i < pNodeAnim->mNumPositionKeys - 1; i++) {
		if (AnimationTime < (float)pNodeAnim->mPositionKeys[i + 1].mTime) {
			return i;
		}
	}
	assert(0);
	return 0;
}

TestModel::Dynamic_Mesh::Dynamic_Mesh(Graphics& gfx, std::vector<std::shared_ptr<Bind::Bindable>> bindPtrs)
{
}

DirectX::XMMATRIX TestModel::Dynamic_Mesh::GetTransformXM() const noexcept
{
	return DirectX::XMMATRIX();
}



