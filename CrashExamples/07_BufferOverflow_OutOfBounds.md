# 07. Buffer Overflow / Out-of-Bounds Access (버퍼 오버플로 / 범위 밖 접근)

> **배열이나 버퍼의 유효 범위를 벗어나 접근하여 메모리 손상 또는 크래시를 유발하는 유형**

---

## 개요

배열 인덱스, 벡터 접근, 고정 크기 버퍼 등에서 범위를 벗어나 접근하면
다른 변수를 덮어쓰거나(heap/stack corruption), 보호된 메모리에 접근하여 크래시합니다.

---

## 유형 A: 빈 벡터 접근

### TigerEngine (JesaSang) - B-5

```cpp
// BAD - 빈 벡터에서 &[0] 접근
void Mesh::CreateBuffers() {
    D3D11_SUBRESOURCE_DATA vertexData = {};
    vertexData.pSysMem = &vertices[0];  // vertices가 비어있으면 UB!

    D3D11_BUFFER_DESC desc = {};
    desc.ByteWidth = sizeof(Vertex) * vertices.size();
    // ByteWidth가 0이면 D3D11 CreateBuffer 실패

    device->CreateBuffer(&desc, &vertexData, &vb);
}
```

### AliceEngine - 1.1

```cpp
// BAD - back() 호출 전 empty 체크 없음
m_NodeIndexOfName[m_Skeleton.back().name] = idx;
// m_Skeleton이 비어있으면 .back() → UB → 크래시

return m_scripts[id].back();
// m_scripts[id] 벡터가 비어있으면 .back() → UB → 크래시
```

### AliceEngine - 2.3

```cpp
// BAD - 빈 벡터의 .data() 사용
skinned.boneMatrices = animComp.palette.data();
skinned.boneCount = static_cast<uint32_t>(animComp.palette.size());
// palette가 비어있으면 .data()는 nullptr → Renderer에서 크래시
```

### GOOD

```cpp
// GOOD - empty 체크
void Mesh::CreateBuffers() {
    if (vertices.empty() || indices.empty()) return;
    // 또는 .data() 사용 (빈 벡터에서 nullptr 반환, &[0]보다 안전)
    vertexData.pSysMem = vertices.data();
}

// GOOD - .back() 전 empty 체크
if (!m_Skeleton.empty()) {
    m_NodeIndexOfName[m_Skeleton.back().name] = idx;
}

// GOOD - boneCount 체크
if (!animComp.palette.empty()) {
    skinned.boneMatrices = animComp.palette.data();
    skinned.boneCount = static_cast<uint32_t>(animComp.palette.size());
} else {
    skinned.boneMatrices = nullptr;
    skinned.boneCount = 0;
}
```

---

## 유형 B: Off-by-One 경계 검사

### TigerEngine - S-6 (CameraSystem)

```cpp
// BAD - > 대신 >= 사용해야 함
void CameraSystem::SetMainCamera(size_t index) {
    if (index > registered.size()) return;  // index == size() 일 때 통과!
    registered[index]->SetMain(true);       // registered[size()] → OOB!
}
```

### TigerEngine - S-7

```cpp
// BAD - 빈 벡터에서 [0] 접근
Camera* CameraSystem::GetCurrCamera() {
    if (currentCamera >= 0) {
        return registered[currentCamera];
    }
    return registered[0];  // registered가 비어있으면 OOB!
}
```

### TigerEngine - S-8 (unsigned 언더플로)

```cpp
// BAD - unsigned에서 -1 = 최대값
void CameraSystem::PrevCamera() {
    currentCamera--;
    if (currentCamera < 0) {
        currentCamera = registered.size() - 1;
        // registered가 비어있으면 size() - 1 = SIZE_MAX!
        // → registered[SIZE_MAX] → OOB 크래시
    }
}
```

### MiKuEngine - C-11 (GPU에서 읽은 ID)

```cpp
// BAD - GPU 피킹 결과가 범위를 벗어날 수 있음
uint32_t picked = ReadPickingBuffer();
auto* go = m_components[picked - 1]->GetGameObject();
// picked가 m_components.size()보다 크면? → OOB!
// stale ID (렌더러 Unregister 후 피킹 버퍼에 남은 ID)
```

### GOOD

```cpp
// GOOD - 정확한 경계 검사
void CameraSystem::SetMainCamera(size_t index) {
    if (index >= registered.size()) return;  // >= 사용!
    registered[index]->SetMain(true);
}

Camera* CameraSystem::GetCurrCamera() {
    if (registered.empty()) return nullptr;
    if (currentCamera >= 0 && currentCamera < registered.size()) {
        return registered[currentCamera];
    }
    return registered[0];
}

void CameraSystem::PrevCamera() {
    if (registered.empty()) return;
    if (currentCamera <= 0) {
        currentCamera = static_cast<int>(registered.size()) - 1;
    } else {
        currentCamera--;
    }
}

// GOOD - GPU 피킹 범위 검사
if (picked > 0 && picked <= m_components.size()) {
    auto* go = m_components[picked - 1]->GetGameObject();
}
```

---

## 유형 C: 고정 크기 배열 오버플로

### TigerEngine - C-13, M-9

```cpp
// BAD - 고정 크기 bone 배열에 제한 없이 쓰기
struct BonePoseBuffer {
    Matrix bonePose[128];  // 128개 고정
};

void FBXRenderer::UpdateBonePoses() {
    for (int i = 0; i < skeleton->GetBoneCount(); i++) {
        bonePoses.bonePose[i] = ...;  // boneCount > 128이면 오버플로!
    }
}
```

```cpp
// BAD - boneOffset 고정 배열에 본 수 제한 없이 쓰기
Matrix boneOffset[MAX_BONES];  // MAX_BONES = 256
for (int i = 0; i < mesh->mNumBones; i++) {
    boneOffset[i] = ...;  // mNumBones > MAX_BONES면 스택 오버플로!
}
```

### TigerEngine - B-14

```cpp
// BAD - 256자 고정 버퍼 2개를 연결
wchar_t buffer[256];
swprintf_s(buffer, 256, L"[ERROR] %s:%d - ", __FUNCTIONW__, __LINE__);
wchar_t message[256];
swprintf_s(message, 256, fmt, ...);
wcscat_s(buffer, 256, message);
// buffer(256자) + message(256자) = 512자 → buffer 오버플로 가능!
```

### AliceEngine - 3.3

```cpp
// BAD - 동일 패턴
wchar_t buffer[256];
swprintf_s(buffer, 256, L"[ERROR] %s:%d - ", __FUNCTIONW__, __LINE__);
wchar_t message[256];
sprintf_s(message, 256, __VA_ARGS__);
// 긴 함수명 + 긴 메시지 → 잘림 또는 오버플로
```

### GOOD

```cpp
// GOOD - 본 수 제한
void FBXRenderer::UpdateBonePoses() {
    int boneCount = std::min(skeleton->GetBoneCount(), MAX_BONES);
    for (int i = 0; i < boneCount; i++) {
        bonePoses.bonePose[i] = ...;
    }
    if (skeleton->GetBoneCount() > MAX_BONES) {
        LOG_WARNING("Bone count %d exceeds MAX_BONES %d",
            skeleton->GetBoneCount(), MAX_BONES);
    }
}

// GOOD - 동적 버퍼 사용
std::wstring FormatError(const wchar_t* func, int line, const wchar_t* fmt, ...) {
    std::wstring result = std::format(L"[ERROR] {}:{} - ", func, line);
    // ... 동적 크기 문자열로 안전
    return result;
}
```

---

## 유형 D: 외부 데이터 기반 인덱스 미검증

### TigerEngine - M-8

```cpp
// BAD - FBX 파일에서 읽은 vertexId 범위 미검사
for (int j = 0; j < pMesh->mNumBones; j++) {
    for (int k = 0; k < bone->mNumWeights; k++) {
        int vertexId = bone->mWeights[k].mVertexId;
        vertices[vertexId].boneWeights = ...;
        // 손상된 FBX에서 vertexId가 vertices.size() 이상이면 OOB!
    }
}
```

### AliceEngine - 2.9

```cpp
// BAD - parent 인덱스 상한 미검사
if (parent >= 0)
    m_Skeleton[parent].children.push_back(idx);
// parent >= 0만 검사, parent < m_Skeleton.size() 상한 미검사!
```

### MiKuEngine - M-09

```cpp
// BAD - JSON에서 읽은 numGameObjects와 실제 크기 불일치 가능
std::vector<GameObject*> idToPtr(numGameObjects + 1);
// JSON 데이터의 ID가 numGameObjects보다 크면 idToPtr[id] OOB!
```

### TigerEngine - B-10 (문자열 리터럴 포인터 산술)

```cpp
// BAD - 문자열 + 정수가 포인터 산술이 됨
throw std::out_of_range("Bone index not Found" + index);
// "Bone index not Found"는 const char[21]
// + index는 포인터를 index바이트 전진!
// index > 20이면 문자열 리터럴 범위 밖 읽기!
```

### GOOD

```cpp
// GOOD - 외부 데이터 인덱스 검증
for (int k = 0; k < bone->mNumWeights; k++) {
    int vertexId = bone->mWeights[k].mVertexId;
    if (vertexId < 0 || vertexId >= static_cast<int>(vertices.size())) {
        LOG_WARNING("Invalid vertex ID %d in bone %s", vertexId, bone->mName.C_Str());
        continue;
    }
    vertices[vertexId].boneWeights = ...;
}

// GOOD - 양방향 범위 검사
if (parent >= 0 && parent < static_cast<int>(m_Skeleton.size())) {
    m_Skeleton[parent].children.push_back(idx);
}

// GOOD - unordered_map 사용
std::unordered_map<int, GameObject*> idToPtr;

// GOOD - std::to_string 사용
throw std::out_of_range("Bone index not Found: " + std::to_string(index));
```

---

## 유형 E: memcpy 크기 미검증

### AliceEngine - 1.4

```cpp
// BAD - 소스 버퍼 크기를 검증하지 않고 memcpy
std::memcpy(&outHdr, raw.data(), sizeof(AliceChunkHeader));
// raw.size() < sizeof(AliceChunkHeader)이면 버퍼 오버리드!
// → 가비지 데이터 읽기 또는 크래시
```

### AliceEngine - 2.10 (FontAtlasBuild)

```cpp
// BAD - memcpy 시 대상/소스 크기 불일치 가능
memcpy(dest, src, width * height * 4);
// 실제 dest 버퍼가 width * height * 4보다 작으면 힙 오버플로
```

### GOOD

```cpp
// GOOD - 크기 검증 후 memcpy
if (raw.size() >= sizeof(AliceChunkHeader)) {
    std::memcpy(&outHdr, raw.data(), sizeof(AliceChunkHeader));
} else {
    LOG_ERROR("Buffer too small for chunk header: %zu < %zu",
        raw.size(), sizeof(AliceChunkHeader));
    return false;
}
```

---

## 발견 프로젝트별 분포

| 프로젝트 | Buffer/OOB 이슈 수 | 대표 사례 |
|----------|---------------------|----------|
| TigerEngine | ~10건 | 빈 벡터, off-by-one, bone 배열, 포인터 산술 |
| Scoopy | 1건 | waveTable 배열 경계 |
| SPEngine | 0건 | - |
| EGOSIS | 1건 | OverlapSphere maxHits |
| AliceEngine | ~5건 | .back() empty, memcpy, parent 인덱스 |
| MiKuEngine | 2건 | GPU 피킹 ID, JSON 인덱스 |

---

## 핵심 교훈

1. **벡터 접근 전 `empty()` 체크, 인덱스 접근 전 `size()` 비교**
2. **`>` 대신 `>=`로 경계 검사** (off-by-one 방지)
3. **unsigned 타입에서 `size() - 1`은 빈 컨테이너에서 최대값이 된다**
4. **외부 데이터(FBX, JSON, GPU)에서 온 인덱스는 반드시 범위 검증**
5. **고정 크기 배열 대신 `std::vector` + `at()` 사용 고려** (디버그 시 범위 검사)
6. **`memcpy` 전에 소스/대상 버퍼 크기를 반드시 비교하라**
7. **문자열 + 정수는 `std::to_string()`을 사용하라** (포인터 산술 방지)
