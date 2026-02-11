# 02. Use-After-Free / Dangling Pointer (해제 후 사용 / 댕글링 포인터)

> **메모리가 해제된 후에도 포인터가 여전히 그 주소를 가리키고 있어 접근 시 크래시하는 유형**

---

## 개요

객체가 `delete`, `Release()`, 풀 반환, 씬 언로드 등으로 해제된 후에도 포인터가 갱신되지 않아
해제된 메모리에 접근하는 문제입니다. **힙 손상**, **크래시**, **데이터 오염** 등 가장 디버깅이 어려운 버그 유형입니다.

---

## 유형 A: SAFE_RELEASE/SAFE_DELETE가 실제로 동작하지 않음

### TigerEngine (JesaSang) - B-6 / SPEngine (Scoopy 4Q) - C-03

두 프로젝트에서 **동일한 코드 패턴**이 발견되었습니다.

```cpp
// BAD - 포인터를 "값"으로 전달 → 호출자의 포인터는 변경되지 않음
template <typename T>
void SAFE_RELEASE(T* p)      // p는 포인터의 복사본!
{
    if (p) { p->Release(); p = nullptr; }  // 로컬 복사본만 null이 됨
}

template <typename T>
void SAFE_DELETE(T* p)        // p는 포인터의 복사본!
{
    if (p) { delete p; p = nullptr; }      // 로컬 복사본만 null이 됨
}

// 호출 예시
ID3D11Buffer* buffer = CreateBuffer();
SAFE_RELEASE(buffer);
// buffer는 여전히 해제된 메모리를 가리킴! (댕글링 포인터)
SAFE_RELEASE(buffer);  // 두 번째 호출 → double-free 크래시!
```

### GOOD - 참조로 전달

```cpp
// GOOD - 포인터 참조로 전달하여 원본을 null로 설정
template <typename T>
void SAFE_RELEASE(T*& p)     // T*& → 포인터의 참조!
{
    if (p) { p->Release(); p = nullptr; }  // 원본 포인터가 null이 됨
}

template <typename T>
void SAFE_DELETE(T*& p)      // T*& → 포인터의 참조!
{
    if (p) { delete p; p = nullptr; }      // 원본 포인터가 null이 됨
}

// 또는 매크로로 구현 (원본 보장)
#define SAFE_RELEASE(p) do { if (p) { (p)->Release(); (p) = nullptr; } } while(0)
#define SAFE_DELETE(p)  do { if (p) { delete (p); (p) = nullptr; } } while(0)
```

---

## 유형 B: 오브젝트 풀 반환 후 참조 유지

### Scoopy - H03, H04

```cpp
// BAD - 적이 풀로 반환된 후에도 enemyTarget 포인터가 살아있음
void Castle::AutoAttack(float dt) {
    if (enemyTarget == nullptr) {  // ObjPtr인데 == nullptr 비교 (잘못된 방법)
        FindTarget();
        return;
    }
    // 적이 이미 풀로 반환되어 비활성화되었지만, enemyTarget은 여전히 참조 중
    auto epos = enemyTarget->GetTransform()->GetWorldPosition();  // 크래시!
}
```

### Scoopy - C03

```cpp
// BAD - 투사체의 소유자(적)가 풀로 반환된 후 접근
void Arrow::Update() {
    if (!target.IsValid()) {
        // target이 사라졌으므로 화살 회수
        owner->GetComponent<ArrowEnemy>()->ReturnArrow(GetGameObject());
        // owner(적)도 이미 풀로 반환되었으면? → 크래시!
    }
}
```

### GOOD

```cpp
// GOOD - ObjPtr의 IsValid() 사용
void Castle::AutoAttack(float dt) {
    if (!enemyTarget.IsValid()) {  // 올바른 유효성 검사
        enemyTarget = ObjPtr<Enemy>();  // 명시적 초기화
        FindTarget();
        return;
    }
    auto epos = enemyTarget->GetTransform()->GetWorldPosition();
}

// GOOD - owner도 유효성 검사
void Arrow::Update() {
    if (!target.IsValid()) {
        if (owner.IsValid()) {
            if (auto* arrowEnemy = owner->GetComponent<ArrowEnemy>()) {
                arrowEnemy->ReturnArrow(GetGameObject());
            }
        }
        Destroy(GetGameObject());  // 고아 투사체 정리
    }
}
```

---

## 유형 C: 씬 리로드 후 댕글링 포인터

### TigerEngine - E-12

```cpp
// BAD - 씬 리로드 시 selectedObject 초기화 안 됨
class Editor {
    GameObject* selectedObject;  // raw pointer

    void ReloadScene() {
        sceneSystem->LoadScene("Level1");
        // selectedObject는 이전 씬의 파괴된 오브젝트를 가리킴!
    }

    void DrawInspector() {
        if (selectedObject) {  // null이 아니지만 이미 파괴된 메모리
            selectedObject->GetName();  // Use-After-Free!
        }
    }
};
```

### MiKuEngine - H-06

```cpp
// BAD - 플레이어 파괴 후에도 raw pointer 유지
class MonsterScript {
    PlayerControllerScript* m_targetPlayer;  // raw pointer

    void Update() {
        // 플레이어가 파괴되면 m_targetPlayer는 댕글링 포인터
        float dist = GetDistanceTo(m_targetPlayer->GetPosition());  // 크래시!
    }
};
```

### MiKuEngine - H-02

```cpp
// BAD - 벡터의 raw pointer를 별도 저장
class BossPatternManager {
    std::vector<std::unique_ptr<BossPattern>> m_patterns;
    std::vector<BossPattern*> m_normalPatterns;  // m_patterns의 raw pointer 저장

    void Init() {
        m_patterns.push_back(std::make_unique<PatternA>());
        m_normalPatterns.push_back(m_patterns.back().get());
        // m_patterns가 재할당되면 m_normalPatterns의 포인터가 댕글링!
    }
};
```

### GOOD

```cpp
// GOOD - Handle 기반 약한 참조 사용
class Editor {
    engine::Ptr<GameObject> selectedObject;  // Handle 기반

    void ReloadScene() {
        sceneSystem->LoadScene("Level1");
        selectedObject = {};  // 명시적 초기화
    }

    void DrawInspector() {
        if (selectedObject.IsValid()) {  // 유효성 검사
            selectedObject->GetName();
        }
    }
};

// GOOD - engine::Ptr 사용
class MonsterScript {
    engine::Ptr<PlayerControllerScript> m_targetPlayer;

    void Update() {
        if (!m_targetPlayer.IsValid()) return;
        float dist = GetDistanceTo(m_targetPlayer->GetPosition());
    }
};

// GOOD - 인덱스 기반 참조
class BossPatternManager {
    std::vector<std::unique_ptr<BossPattern>> m_patterns;
    std::vector<size_t> m_normalPatternIndices;  // 인덱스로 참조

    BossPattern* GetNormalPattern(size_t i) {
        size_t idx = m_normalPatternIndices[i];
        if (idx < m_patterns.size()) return m_patterns[idx].get();
        return nullptr;
    }
};
```

---

## 유형 D: DLL 언로드 후 vtable 접근

### EGOSIS - CR-03

```cpp
// BAD - FreeLibrary 후 스크립트 인스턴스의 vtable이 무효화
void ScriptHotReload_Unload() {
    ::FreeLibrary(g_ScriptModule);  // DLL 코드가 메모리에서 제거됨
    SetDynamicScriptFunctions(nullptr, nullptr, nullptr);
    // 기존 스크립트 인스턴스의 vtable 포인터가 언로드된 DLL 영역을 가리킴!
}

// 이후 어딘가에서...
script->Update();  // vtable을 통한 가상 함수 호출 → 해제된 코드 영역 접근 → 크래시!
```

### GOOD

```cpp
// GOOD - DLL 언로드 전에 모든 스크립트 인스턴스를 먼저 파괴
void ScriptHotReload_Unload() {
    // 1. 모든 스크립트 인스턴스 파괴
    World::RemoveAllScripts();

    // 2. 스크립트 인스턴스가 모두 사라진 것을 확인 후 DLL 언로드
    ::FreeLibrary(g_ScriptModule);
    SetDynamicScriptFunctions(nullptr, nullptr, nullptr);
}
```

---

## 유형 E: 함수 반환 후 지역 변수 참조

### Scoopy - H05

```cpp
// BAD - 임시 객체에 대한 참조 반환
const std::wstring& LevelUpManager::GetHeadline(int id) {
    auto it = headlines.find(id);
    if (it != headlines.end()) {
        return it->second;
    }
    return L"";  // 임시 wstring 생성 후 즉시 파괴 → 댕글링 참조!
}
```

### SPEngine (Scoopy 4Q) - M-01

```cpp
// BAD - 지역 객체의 포인터를 반환
LPCWSTR GetComErrorString(HRESULT hr) {
    _com_error err(hr);                    // 지역 객체
    LPCWSTR errMsg = err.ErrorMessage();   // 지역 객체의 내부 포인터
    return errMsg;                          // err 소멸 → errMsg 댕글링!
}
```

### GOOD

```cpp
// GOOD - 값으로 반환
std::wstring LevelUpManager::GetHeadline(int id) {
    auto it = headlines.find(id);
    if (it != headlines.end()) {
        return it->second;  // 복사 반환 (또는 move)
    }
    return L"";  // 값 반환이므로 안전
}

// GOOD - 또는 static 빈 문자열 사용
const std::wstring& LevelUpManager::GetHeadline(int id) {
    static const std::wstring empty;
    auto it = headlines.find(id);
    if (it != headlines.end()) return it->second;
    return empty;  // static이므로 수명 보장
}

// GOOD - wstring으로 반환
std::wstring GetComErrorString(HRESULT hr) {
    _com_error err(hr);
    return std::wstring(err.ErrorMessage());  // 복사 후 반환
}
```

---

## 유형 F: Physics Handle의 Stale 포인터

### AliceEngine - 1.3

```cpp
// BAD - PhysX actor 포인터를 정수 핸들로 변환 후 역변환
out.actorHandle = h.actor
    ? static_cast<uint64_t>(reinterpret_cast<uintptr_t>(h.actor))
    : 0ull;

// 나중에 역변환...
PxActor* actor = reinterpret_cast<PxActor*>(static_cast<uintptr_t>(actorHandle));
actor->getGlobalPose();  // actor가 이미 삭제되었으면 Use-After-Free!
```

### EGOSIS - ME-12

```cpp
// BAD - 이전 프레임의 OverlapHit에서 가져온 native 포인터를 다음 프레임에 사용
PxActor* cachedActor = previousFrameHit.nativeActor;
// 다음 프레임에서 actor가 삭제되었을 수 있음
ComputePenetration(cachedActor, ...);  // 댕글링 포인터!
```

### GOOD

```cpp
// GOOD - generation counter로 유효성 검증
struct PhysicsHandle {
    uint64_t ptr;
    uint32_t generation;
};

PxActor* ResolveHandle(PhysicsHandle handle) {
    auto* actor = reinterpret_cast<PxActor*>(static_cast<uintptr_t>(handle.ptr));
    if (GetGeneration(actor) != handle.generation) return nullptr;  // stale!
    return actor;
}

// GOOD - EntityId 기반 조회
void ComputePenetration(EntityId entityId, ...) {
    auto* actor = physicsWorld.GetActorForEntity(entityId);
    if (!actor) return;  // 엔티티가 삭제되었으면 안전하게 무시
    // ...
}
```

---

## 발견 프로젝트별 분포

| 프로젝트 | Use-After-Free 이슈 수 | 대표 사례 |
|----------|------------------------|----------|
| TigerEngine | ~10건 | SAFE_RELEASE 값 전달, 씬 리로드 댕글링 |
| Scoopy | ~6건 | 오브젝트 풀 반환 후 참조, 임시 객체 참조 반환 |
| SPEngine | ~3건 | SAFE_RELEASE 값 전달, 지역 객체 포인터 반환 |
| EGOSIS | ~4건 | DLL 언로드 후 vtable, stale native pointer |
| AliceEngine | ~3건 | Physics handle, weak_ptr 미검증 |
| MiKuEngine | ~5건 | raw pointer 댕글링, 풀 반환 후 접근 |

---

## 핵심 교훈

1. **포인터를 null로 설정하는 함수는 반드시 참조(`T*&`)로 받아라**
2. **오브젝트 풀 사용 시 반환된 객체의 모든 참조를 무효화하라**
3. **씬 전환/리로드 시 모든 캐시된 포인터를 초기화하라**
4. **DLL 핫리로드 전 모든 인스턴스를 먼저 파괴하라**
5. **지역 변수의 포인터/참조를 함수 밖으로 반환하지 마라**
6. **raw pointer 대신 Handle 기반 약한 참조 시스템을 사용하라**
