# 04. Memory Leak (메모리 누수)

> **할당된 메모리가 해제되지 않아 점진적으로 메모리가 고갈되는 유형**

---

## 개요

`new`로 할당한 메모리를 `delete`하지 않거나, COM 리소스를 `Release()`하지 않으면
메모리가 누적되어 결국 OOM(Out of Memory) 크래시 또는 성능 저하를 유발합니다.

---

## 유형 A: 소멸자 호출만 하고 메모리 해제 안 함

### TigerEngine (JesaSang) - B-1

```cpp
// BAD - new로 할당했지만 delete가 아닌 소멸자만 호출
void ObjectSystem::Destory(Handle h) {
    auto& slot = slots[h.index];
    slot.ptr->~Object();    // 소멸자만 호출
    slot.ptr = nullptr;      // 메모리는 해제되지 않음!
    // delete slot.ptr; 이 빠져있음
}
```

**왜 위험한가:**
- 소멸자 호출(`~Object()`)은 멤버 정리만 수행
- 실제 메모리 반환은 `delete`(또는 `operator delete`)가 담당
- 오브젝트가 생성/파괴될 때마다 메모리 누수 누적

### GOOD

```cpp
// GOOD - delete로 소멸자 호출 + 메모리 해제
void ObjectSystem::Destroy(Handle h) {
    auto& slot = slots[h.index];
    delete slot.ptr;         // 소멸자 호출 + 메모리 해제
    slot.ptr = nullptr;
}

// GOOD - placement new를 사용했다면 소멸자 + free
void ObjectSystem::Destroy(Handle h) {
    auto& slot = slots[h.index];
    slot.ptr->~Object();            // placement new였다면 소멸자만 호출하는 것이 맞음
    pool.deallocate(slot.ptr);       // 풀에 메모리 반환
    slot.ptr = nullptr;
}
```

---

## 유형 B: FSM 상태 객체 new 후 미해제

### TigerEngine - W-46~W-49

```cpp
// BAD - new로 생성한 FSM 상태를 소멸자에서 해제하지 않음
void PlayerController::Init() {
    fsmStates[0] = new Player_Idle(this);
    fsmStates[1] = new Player_Walk(this);
    fsmStates[2] = new Player_Run(this);
    fsmStates[3] = new Player_Hit(this);
    fsmStates[4] = new Player_Cook(this);
    fsmStates[5] = new Player_Interact(this);
    fsmStates[6] = new Player_MiniGame(this);
    fsmStates[7] = new Player_Altar(this);
    fsmStates[8] = new Player_Dead(this);
    // 소멸자에 delete가 없음 → 9개 상태 객체 누수
}

// AdultGhostController도 동일 (5개 상태)
// BabyGhostController도 동일 (4개 상태)
// TutorialController도 동일 (9개 스텝)
```

### GOOD

```cpp
// GOOD - unique_ptr 사용
class PlayerController {
    std::unique_ptr<IState> fsmStates[9];

    void Init() {
        fsmStates[0] = std::make_unique<Player_Idle>(this);
        fsmStates[1] = std::make_unique<Player_Walk>(this);
        // ... 소멸자에서 자동 해제
    }
};

// GOOD - 또는 소멸자에서 명시적 해제
PlayerController::~PlayerController() {
    for (auto& state : fsmStates) {
        delete state;
        state = nullptr;
    }
}
```

---

## 유형 C: 싱글턴 new 후 delete 없음

### SPEngine (Scoopy 4Q) - H-01

```cpp
// BAD - new로 생성하지만 delete 호출 코드 없음
AssimpLoader* AssimpLoader::instance = nullptr;

AssimpLoader* AssimpLoader::GetInstance() {
    if (instance == nullptr) {
        instance = new AssimpLoader();  // 할당
    }
    return instance;
}
// delete instance; 를 호출하는 코드가 프로젝트 어디에도 없음
// → Assimp::Importer 등 내부 리소스도 정리되지 않음
```

### GOOD

```cpp
// GOOD - Meyer's Singleton (static local)
AssimpLoader& AssimpLoader::GetInstance() {
    static AssimpLoader instance;  // 프로그램 종료 시 자동 소멸
    return instance;
}

// GOOD - unique_ptr 싱글턴
class AssimpLoader {
    static std::unique_ptr<AssimpLoader> instance;
public:
    static AssimpLoader* GetInstance() {
        if (!instance) instance = std::make_unique<AssimpLoader>();
        return instance.get();
    }
    static void Shutdown() { instance.reset(); }
};
```

---

## 유형 D: COM/DirectX 리소스 Release 누락

### TigerEngine - M-10~M-12

```cpp
// BAD - Pixel Shader Blob를 Release하지 않음
ID3DBlob* pixelShaderBuffer = nullptr;
HR_T(D3DCompileFromFile(path, nullptr, nullptr, "main", "ps_5_0",
    0, 0, &pixelShaderBuffer, nullptr));

HR_T(device->CreatePixelShader(
    pixelShaderBuffer->GetBufferPointer(),
    pixelShaderBuffer->GetBufferSize(),
    nullptr, &pixelShader));

// pixelShaderBuffer->Release(); 가 빠져있음!
// → 셰이더 컴파일할 때마다 Blob 메모리 누수
```

### SPEngine (Scoopy 4Q) - H-06

```cpp
// BAD - UnInitD3D()가 빈 함수
void RenderPipe::UnInitD3D() {
    // 아무것도 안 함!
    // SwapChain, Device, DeviceContext 등 모든 D3D 리소스 미해제
}
```

### GOOD

```cpp
// GOOD - ComPtr 사용으로 자동 Release
Microsoft::WRL::ComPtr<ID3DBlob> pixelShaderBuffer;
HR_T(D3DCompileFromFile(path, nullptr, nullptr, "main", "ps_5_0",
    0, 0, pixelShaderBuffer.GetAddressOf(), nullptr));
// 스코프 벗어나면 자동 Release

// GOOD - 또는 수동 Release 보장
ID3DBlob* pixelShaderBuffer = nullptr;
HR_T(D3DCompileFromFile(..., &pixelShaderBuffer, nullptr));
HR_T(device->CreatePixelShader(...));
pixelShaderBuffer->Release();  // 사용 후 즉시 해제
pixelShaderBuffer = nullptr;
```

---

## 유형 E: 캐시 미적용으로 인한 반복 로딩

### TigerEngine - M-17

```cpp
// BAD - 캐시 없이 매번 디스크에서 재로드
FBXAsset* FBXResourceManager::LoadStaticFBXByPath(const std::string& path) {
    // 캐시 검색 없음!
    // 매 호출마다 파일 읽기 + Assimp 파싱 + GPU 업로드
    auto* asset = new FBXAsset();
    // ... 로딩 로직
    return asset;
    // 이전에 로드한 동일 경로의 asset은 해제 없이 방치 → 누수
}
```

### GOOD

```cpp
// GOOD - 캐시 적용
FBXAsset* FBXResourceManager::LoadStaticFBXByPath(const std::string& path) {
    auto it = staticCache.find(path);
    if (it != staticCache.end()) {
        return it->second.get();  // 캐시 히트
    }
    auto asset = std::make_unique<FBXAsset>();
    // ... 로딩 로직
    auto* raw = asset.get();
    staticCache[path] = std::move(asset);
    return raw;
}
```

---

## 유형 F: 노드 교체 시 기존 노드 미해제

### TigerEngine - C-20 (GridComponent::FindPath)

```cpp
// BAD - 같은 위치에 새 노드를 생성하면서 기존 노드 미해제
Node* openNode = new Node(neighbor);
// ... A* 탐색 중
Node* betterNode = new Node(neighbor);  // 같은 위치에 더 좋은 경로 발견
// openNode는 여전히 우선순위 큐에 있지만 참조 교체
grid[x][y] = betterNode;  // 기존 openNode의 메모리 누수!
```

### GOOD

```cpp
// GOOD - 스마트 포인터 또는 풀 사용
std::vector<std::unique_ptr<Node>> nodePool;

Node* AllocNode(const GridPos& pos) {
    nodePool.push_back(std::make_unique<Node>(pos));
    return nodePool.back().get();
}
// FindPath 종료 시 nodePool.clear()로 일괄 해제
```

---

## 유형 G: 가상 소멸자 누락

### TigerEngine - B-11, B-12, W-66

```cpp
// BAD - 다형적 인터페이스에 가상 소멸자 없음
class IRenderer {
public:
    virtual void Render() = 0;
    // virtual ~IRenderer() = default;  ← 이게 없음!
};

class IItem {
public:
    virtual void Use() = 0;
    // virtual ~IItem() = default;  ← 이게 없음!
};

// 사용처
std::unique_ptr<IItem> item = std::make_unique<HealthPotion>();
// item 소멸 시 ~IItem()이 비가상이므로 ~HealthPotion()이 호출되지 않음!
// → HealthPotion의 멤버 리소스 미해제 → 메모리 누수 + UB
```

### GOOD

```cpp
// GOOD - 가상 소멸자 선언
class IRenderer {
public:
    virtual ~IRenderer() = default;
    virtual void Render() = 0;
};

class IItem {
public:
    virtual ~IItem() = default;
    virtual void Use() = 0;
};
```

---

## 발견 프로젝트별 분포

| 프로젝트 | Memory Leak 이슈 수 | 대표 사례 |
|----------|---------------------|----------|
| TigerEngine | ~12건 | ObjectSystem, FSM 상태, Shader Blob, GridComponent |
| Scoopy | 1건 | Arrow 오브젝트 풀 미반환 |
| SPEngine | ~3건 | AssimpLoader 싱글턴, UnInitD3D 미구현 |
| EGOSIS | 1건 | AdvancedAnimator raw new |
| AliceEngine | ~4건 | AdvancedAnimator, ScriptFactory, Scene raw new |
| MiKuEngine | 1건 | StaticMemoryPool heap fallback |

---

## 핵심 교훈

1. **`new`를 사용했으면 반드시 대응하는 `delete`가 있어야 한다 (RAII 원칙)**
2. **`std::unique_ptr`, `std::shared_ptr`를 적극 사용하라**
3. **COM 리소스는 `ComPtr<T>`로 관리하라**
4. **다형적 클래스에는 반드시 `virtual ~Destructor() = default`를 선언하라**
5. **소멸자 호출(`~T()`)과 메모리 해제(`delete`)는 다른 작업이다**
6. **리소스 로딩은 캐시 패턴을 적용하여 중복 할당을 방지하라**
