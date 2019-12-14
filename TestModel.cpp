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
	unsigned int flag = aiProcess_ConvertToLeftHanded	//왼손좌표계로
		| aiProcess_GenNormals							//노멀값 생성
		| aiProcess_CalcTangentSpace					//탄젠트 계산
		| aiProcess_Triangulate							//삼각형으로만들기
		| aiProcess_JoinIdenticalVertices;				//최적화

	m_pScene = m_Importer.ReadFile(filePath, flag);

	if (m_pScene)
	{
		m_GlobalInverseTransform = XMMatrixTranspose((XMMATRIX)m_pScene->mRootNode->mTransformation[0]);
		XMMatrixInverse(nullptr, m_GlobalInverseTransform);
		MakeModel(gfx, filePath, scale);	//모델을 만드는곳
	}

}

TestModel::~TestModel()
{

}

void TestModel::Update(Graphics& gfx,double timeDelta)
{
	// 노드 순회하면서 행렬 업데이트
	BoneTransform(timeDelta, m_BonesTransMat);
	// 트랜스폼 없데이트
	// 임시로 만든 정점쉐이더 0번 상수버퍼에 전달할 행렬들 (모델뷰,모델뷰투영)
	TestModel::Transforms2 tf;
	XMMATRIX modelView = XMMatrixIdentity() * gfx.GetCamera();
	tf.modelView = XMMatrixTranspose(modelView);
	tf.modelViewProj = XMMatrixTranspose(modelView * gfx.GetProjection());

	Transbufff->Update(gfx, tf);

	//위에서 나온 m_boneTransMat 벡터에 담긴 뼈 행렬들을 쉐이더에 전달
	D3D11_MAPPED_SUBRESOURCE msr;
	gfx.GetContext()->Map(
		boneBuffer, 0u,
		D3D11_MAP_WRITE_DISCARD, 0u,
		&msr);
	memcpy(msr.pData,m_BonesTransMat.data(), sizeof(DirectX::XMMATRIX) * m_BonesTransMat.size());
	gfx.GetContext()->Unmap(boneBuffer, 0u);

	// 정점쉐이더 1번슬롯에 전달
	gfx.GetContext()->VSSetConstantBuffers(1, 1u, &boneBuffer);

}

void TestModel::Render(Graphics& gfx)
{	
	// 렌더링시 각각의 바인딩객체들 바인딩 실시
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

	std::vector<XMFLOAT3> positions;				//모델의 정정을 담는 컨테이너
	std::vector<XMFLOAT3> Normal;					//노멀값
	std::vector<VertexBoneData> bones;				//뼈정보
	std::vector<Vertex> vertex;				
	std::vector<UINT>	Indices;					//인덱스

	UINT NumVertices = 0;
	UINT NUMIndices = 0;

	// 정점과 인덱스 갯수 세기
	for (UINT i = 0; i < m_Entries.size(); ++i)
	{
		m_Entries[i].materialIndex = m_pScene->mMeshes[i]->mMaterialIndex;		//해당 메쉬의 재질 인덱스
		m_Entries[i].numIndices	   = m_pScene->mMeshes[i]->mNumFaces * 3;		//해당 메쉬의 인덱스 갯수
		m_Entries[i].baseVertex	   = NumVertices;								//해당 메쉬의 시작 정점
		m_Entries[i].baseIndex	   = NUMIndices;								//해당 메쉬의 시작 인덱스

		NumVertices += m_pScene->mMeshes[i]->mNumVertices;						//해당메쉬 정점 누적 (다음 메쉬의 시작지점)
		NUMIndices  += m_Entries[i].numIndices;									//해당메쉬 인덱스 누적 (다음 메쉬의 시작지점)
	}

	// 메쉬의 정보를 저장하기위해 미리 공간을 할당 (정점,인덱스,노멀,뼈정보등등..)
	positions.reserve(NumVertices);
	Normal.reserve(NumVertices);
	bones.resize(NumVertices);
	vertex.resize(NumVertices);
	Indices.reserve(NUMIndices);

	// 메쉬를 한개씩 초기화한다.
	for (UINT i = 0; i < m_Entries.size(); ++i)
	{
		const aiMesh* paiMesh = m_pScene->mMeshes[i];
		InitMesh(i, paiMesh, positions, Normal, bones, Indices);	//메쉬초기화
	}

	// 초기화가 끝나면 각 컨테이너에는 모델의 정보가 담긴다.
	// 총 점정갯수만큼 정보가공 현재 쉐이더 전달할 정보는 위치,노멀,뼈 ID,가중치이다.
	// 이중 뼈와 가중치는 XMINT4,XMFLOAT4
	for (UINT i = 0; i < NumVertices; ++i)
	{
		XMINT4 boneID = XMINT4(bones[i].IDs);
		XMFLOAT4 weight = XMFLOAT4(bones[i].Weights);
		vertex[i] = { positions[i],Normal[i],boneID,weight };
	}
	
	// 정점을담을 버퍼 생성
	Dvtx::VertexBuffer vbuf(std::move(Dvtx::VertexLayout{})
		.Append(Dvtx::VertexLayout::Position3D).Append(Dvtx::VertexLayout::Normal)
		.Append(Dvtx::VertexLayout::BoneID).Append(Dvtx::VertexLayout::Weight));
	
	// 총 점점갯수만큼 반목문을통해 버퍼에 정보를 채운다.
	for (UINT i = 0; i < NumVertices; ++i)
	{
		vbuf.EmplaceBack(
			XMFLOAT3(positions[i].x * scale, positions[i].y * scale, positions[i].z * scale),				//위치
			XMFLOAT3(Normal[i].x, Normal[i].y , Normal[i].z ),												//노멀
			XMINT4(bones[i].IDs[0], bones[i].IDs[1], bones[i].IDs[2], bones[i].IDs[3]),						//뼈ID
			XMFLOAT4(bones[i].Weights[0], bones[i].Weights[1], bones[i].Weights[2], bones[i].Weights[3]));	//가중치
	}

	//밑의 작업은 일반적인 파이프라인 바인딩 작업
	// 토폴로지
	topology = (Bind::Topology::Resolve(gfx,D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST));
	// 정점버퍼를 버퍼내용으로 생성
	vertexBuf = (Bind::VertexBuffer::Resolve(gfx, "test", vbuf));
	// 인덱스 버퍼 생성
	IndexBuf = (Bind::IndexBuffer::Resolve(gfx, "test2", Indices));
	// 정점 쉐이더 생성
	vertexShader = (Bind::VertexShader::Resolve(gfx, "AniamtionNoTex.cso"));
	// 픽셀 쉐이더 생성
	PixelShader = (Bind::PixelShader::Resolve(gfx, "AnimationPSNoTex.cso"));
	// 입력 레이아웃 생성
	InputLayout = (Bind::InputLayout::Resolve(gfx, vbuf.GetLayout(), vertexShader->GetBytecode()));


	//  본 노드 행렬을 담을 상수버퍼 생성
	D3D11_BUFFER_DESC cbd;
	cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	cbd.Usage = D3D11_USAGE_DYNAMIC;
	cbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	cbd.MiscFlags = 0u;
	cbd.ByteWidth = sizeof(DirectX::XMMATRIX) * 128;			//총 128개의 4x4 행렬을 전달한다
	cbd.StructureByteStride = 0u;

	gfx.GetDevice()->CreateBuffer(&cbd, nullptr, &boneBuffer);	// 본노드행렬 상수버퍼 생성

	// 추가적은 재질및 빛에의한 색깔을 담은정보
	PSMaterialConstantNotex psmc;
	psmc.specularPower = 2.0f;
	psmc.specularColor = { 0.18f,0.18f,0.18f,1.0f };
	psmc.materialColor = { 0.45f,0.45f,0.85f,1.0f };
	// 픽셀쉐이더 상수버퍼 0번쨰에는 이미 빛의 정보에대해 들어가고있으므로(다른코드에서)
	// 비어있는 1번슬롯에 할당
	pixelConst = (Bind::PixelConstantBuffer<TestModel::PSMaterialConstantNotex>::Resolve(gfx, psmc, 1u));
	// 위치를 담은 트랜스폼 버퍼
	Transbufff = (Bind::VertexConstantBuffer<TestModel::Transforms2>::Resolve(gfx, 0u));
	// 뼈버퍼
	bonbufff = (Bind::VertexConstantBuffer<BoneBuffer>::Resolve(gfx,1u));
}

void TestModel::InitMesh(UINT meshIndex, const aiMesh* paiMesh, std::vector<XMFLOAT3>& Positions, 
						std::vector<XMFLOAT3>& Normals, 
						std::vector<TestModel::VertexBoneData>& Bones,
						std::vector<UINT>& Indices)
{
	const aiVector3D Zero3D(0.0f, 0.0f, 0.0f);

	// 메쉬의 정점을 생산한다.
	for (UINT i = 0; i < paiMesh->mNumVertices; ++i)
	{
		const aiVector3D* pPos = &(paiMesh->mVertices[i]);
		const aiVector3D* pNormal = &(paiMesh->mNormals[i]);

		Positions.push_back(XMFLOAT3(pPos->x, pPos->y, pPos->z));
		Normals.push_back(XMFLOAT3(pNormal->x, pNormal->y, pNormal->z));
	}

	// 뼈정보를 로드한다.
	LoadBone(meshIndex, paiMesh, Bones);

	// 인덱스 버퍼를 만든다
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
	// 메쉬가 가진 뼈의 정보에 접근
	for (UINT i = 0; i < paiMesh->mNumBones; ++i)
	{
		UINT boneIndex = 0;											//뼈의 인덱스
		std::string boneName(paiMesh->mBones[i]->mName.C_Str());	//뼈의 이름

		// 해당 뼈가 맵에 있는지 검사
		if (m_BoneMapping.find(boneName) == m_BoneMapping.end())
		{
			// 없을경우 현재 뼈 갯수가 새로운 인덱스가된다.
			boneIndex = m_NumBones;	
			m_NumBones++;
			Bone bi;
			m_BoneInfo.push_back(bi);
			// 뼈의 오프셋행렬을 저장한다.
			m_BoneInfo[boneIndex].BoneOffset = XMMatrixTranspose((XMMATRIX)paiMesh->mBones[i]->mOffsetMatrix[0]);
			m_BoneMapping[boneName] = boneIndex;
		}
		else //기존에 있을경우 인덱스만 반환
			boneIndex = m_BoneMapping[boneName];

		// 해당뼈의 가중치 갯수만큼 반목문을 수행한다.
		for (UINT j = 0; j < paiMesh->mBones[i]->mNumWeights; ++j)
		{
			// 현재 메쉬의 기반 정점부터 뼈의 가중치와 정점 ID 구조체를 사용하여
			// 해당 메쉬의 정점들에 뼈ID와 가중치를 전달한다. 
			UINT vertexID = m_Entries[meshIndex].baseVertex + paiMesh->mBones[i]->mWeights[j].mVertexId;
			float weight = paiMesh->mBones[i]->mWeights[j].mWeight;
			Bones[vertexID].AddBoneData(boneIndex, weight);
		}
	}
}

void TestModel::InitMaterial(const std::string& fileName)
{
	//aiMesh는 말그대로 여러정점과 인덱스로 이루어진 기하구조이다.
	using namespace std::string_literals;
	using namespace Bind;
	using Dvtx::VertexLayout;
	//모델 경로.
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
	//	//텍스처가 없다면 머터리얼에서 색깔값을 가져온다.
	//	material.Get(AI_MATKEY_COLOR_DIFFUSE, reinterpret_cast<aiColor3D&>(diffuseColor));
	//	////머터리얼에서 Diffuse 텍스처 정보를 가져온다
	//	//if (material.GetTexture(aiTextureType_DIFFUSE, 0, &texFileName) == aiReturn_SUCCESS)
	//	//{
	//	//	//해당 텍스쳐 푸쉬
	//	//	bindablePtrs.push_back(Texture::Resolve(gfx, rootPath + texFileName.C_Str()));
	//	//	hasDiffuseMap = true;
	//	//}
	//	//else
	//	//	//텍스처가 없다면 머터리얼에서 색깔값을 가져온다.
	//	//	material.Get(AI_MATKEY_COLOR_DIFFUSE, reinterpret_cast<aiColor3D&>(diffuseColor));

	//	////스페큘러 맵 정보를 가져온다
	//	//if (material.GetTexture(aiTextureType_SPECULAR, 0, &texFileName) == aiReturn_SUCCESS)
	//	//{
	//	//	auto tex = Texture::Resolve(gfx, rootPath + texFileName.C_Str(), 1u);
	//	//	hasAlphaGloss = tex->HasAlhpa();
	//	//	bindablePtrs.push_back(std::move(tex));
	//	//	hasSpecularMap = true;
	//	//}
	//	//else
	//	//	//스페큘러 맵이 없다면.

	//	////가져온 스페큘러맵에 알파값이없을경우 기본 값으로
	//	//	if (!hasAlphaGloss)
	//	//		material.Get(AI_MATKEY_SHININESS, shininess);

	//	////노멀 맵 정보를 가져온다.
	//	//if (material.GetTexture(aiTextureType_NORMALS, 0, &texFileName) == aiReturn_SUCCESS)
	//	//{
	//	//	auto tex = Texture::Resolve(gfx, rootPath + texFileName.C_Str(), 2u);
	//	//	hasAlphaGloss = tex->HasAlhpa();
	//	//	bindablePtrs.push_back(std::move(tex));
	//	//	hasNormalMap = true;
	//	//}
	//	////3중 하나라도 있으면 샘플러 추가.
	//	//if (hasDiffuseMap || hasSpecularMap || hasNormalMap)
	//	//	bindablePtrs.push_back(Bind::Sampler::Resolve(gfx));
	//}
}

void TestModel::BoneTransform(double timeInSeconds, std::vector<XMMATRIX>& transforms)
{
	XMMATRIX Identity = XMMatrixIdentity();

	// 애니메이션을 위한 변수들
	double ticksPerSeconds = m_pScene->mAnimations[0]->mTicksPerSecond != 0 ? m_pScene->mAnimations[0]->mTicksPerSecond : 25.0f;
	double timeInTicks = timeInSeconds * ticksPerSeconds;
	double aniamtionTime = fmod(timeInTicks, m_pScene->mAnimations[0]->mDuration);

	//이 함수가 재귀되어 노드들을 순회하면서 노드행렬을 업데이트한다
	ReadNodeHeirarchy(aniamtionTime, m_pScene->mRootNode, Identity);

	//본 개수만큼 트랜스폼 컨테이너 크기조정
	transforms.resize(m_NumBones);

	for (UINT i = 0; i < m_NumBones; ++i)
		transforms[i] = XMMatrixTranspose(m_BoneInfo[i].FinalTransformation);
		//transforms[i] = XMMatrixTranspose(XMMatrixTranslation(-7.0f,0.0f,0.0f));
		//transforms[i] = XMMatrixIdentity();


}

void TestModel::ReadNodeHeirarchy(double animationTime, const aiNode* pNode, const XMMATRIX& parentTransform)
{
	
	const std::string& nodeName = pNode->mName.C_Str();  //노드 이름
	
	// 현재 지정된 애니메이션 (첫번쨰)
	const aiAnimation* pAnimation = m_pScene->mAnimations[0];								// 씬의 첫번째 애니메이션을 가져온다.
	XMMATRIX nodeTransformation = XMMatrixTranspose(XMMATRIX(pNode->mTransformation[0]));	// 노드의 트랜스폼 행렬을 가져온다(로컬)
	const aiNodeAnim* pNodeAnim = FindNodeAnim(pAnimation, nodeName);						// 현재 노드가 애니메이션이 있는지 확인

	// 만약 노드에 애니메이션이 있다면
	if (pNodeAnim)
	{
		//스케일값 보간
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

		// 각각의 값들을 종합하여 전치 ( 애니메이션이 있을경우 해당 값이 로컬 행렬이 된다)
		nodeTransformation = XMMatrixTranspose(XMLoadFloat4x4(
			reinterpret_cast<const XMFLOAT4X4*>(&mat)
		));
	}
	// 부모로부터의 행렬과 자식행렬을 합쳐 글로벌 행렬 만들기
	XMMATRIX GlobalTransformation = nodeTransformation* parentTransform;

	// 해당 노드가 뼈노드 일경우.
	if (m_BoneMapping.find(nodeName) != m_BoneMapping.end())
	{
		// 있다면 뼈로 이동시키기위한 오프셋행렬도 같이 연산해준다.
		UINT boneIndex = m_BoneMapping[nodeName];
		m_BoneInfo[boneIndex].FinalTransformation = m_BoneInfo[boneIndex].BoneOffset*GlobalTransformation *m_GlobalInverseTransform; //뼈의 오프셋과함께 곱해준다.
		int a = 10;
	}

	// 자식또 똑같이 노드를 업데이트 한다.
	for (UINT i = 0; i < pNode->mNumChildren; ++i)
		ReadNodeHeirarchy(animationTime, pNode->mChildren[i], GlobalTransformation);
	
}

const aiNodeAnim* TestModel::FindNodeAnim(const aiAnimation* pAnimation, const std::string& nodeName)
{
	// 해당 애니메이션이 얼마만큼의 노드를 움직이는것이 mNumChannels 이다.
	for (UINT i = 0; i < pAnimation->mNumChannels; ++i)
	{
		// 애니메이션의 각채널에 접근하여 애니메이션 노드의 포인터를 받아온다
		const aiNodeAnim* pNodeAnim = pAnimation->mChannels[i];

		// 해당 애님노드의 이름이 뼈노드와 이름이 같다면 애니메이션 노드인것이므로 반환.
		if (std::string(pNodeAnim->mNodeName.data) == nodeName)
			return pNodeAnim;
	}
	return nullptr;
}

//스케일 보간
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
//회전 보간
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

//이동보간
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

// 해당 노드의 애니메이션에서 크기변화 에대한 키값이 존재하는지 검사
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
// 해당 노드의 애니메이션에서 회전에대한 키값이 존재하는지 검사
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
// 해당 노드의 애니메이션에서 이동에대한 키값이 존재하는지 검사
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



