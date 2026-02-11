# 08. Race Condition / Thread Safety (경쟁 조건 / 스레드 안전성)

> **다중 스레드 또는 재진입 상황에서 동기화 없이 공유 데이터에 접근하여 데이터 손상이나 크래시가 발생하는 유형**

---

## 개요

게임 엔진에서는 렌더링, 물리, 오디오, 리소스 로딩 등이 별도 스레드에서 실행될 수 있습니다.
공유 데이터에 대한 동기화 없이 동시 접근하면 **데이터 레이스**, **데드락**, **크래시**가 발생합니다.

---

## 유형 A: static 버퍼를 여러 스레드가 공유

### TigerEngine (JesaSang) - B-13 / SPEngine (Scoopy 4Q) - H-07

```cpp
// BAD - static 로컬 버퍼를 모든 스레드가 공유
class com_exception : public std::exception {
    const char* what() const noexcept override {
        static char s_str[64] = {};  // 모든 인스턴스/스레드가 공유!
        sprintf_s(s_str, "Failure with HRESULT of %08X",
            static_cast<unsigned int>(result));
        return s_str;
        // 스레드 A가 s_str에 쓰는 동안 스레드 B가 읽으면?
        // → 데이터 레이스 → 가비지 문자열 또는 크래시
    }
};
```

### TigerEngine - C-25 (static 랜덤 엔진)

```cpp
// BAD - static 랜덤 엔진이 스레드 안전하지 않음
Vector3 AgentComponent::PickRandomTarget() {
    static std::mt19937 gen(std::random_device{}());
    static std::uniform_real_distribution<float> dist(-range, range);
    return Vector3(dist(gen), 0, dist(gen));
    // mt19937은 thread-safe하지 않음
    // 여러 Agent가 동시에 호출하면 내부 상태 손상
}
```

### GOOD

```cpp
// GOOD - thread_local 사용
const char* what() const noexcept override {
    thread_local char s_str[64] = {};  // 스레드마다 별도 버퍼
    sprintf_s(s_str, "Failure with HRESULT of %08X",
        static_cast<unsigned int>(result));
    return s_str;
}

// GOOD - 멤버 문자열로 저장
class com_exception : public std::exception {
    std::string m_msg;
public:
    com_exception(HRESULT hr) {
        m_msg = std::format("Failure with HRESULT of {:08X}", (unsigned)hr);
    }
    const char* what() const noexcept override { return m_msg.c_str(); }
};

// GOOD - thread_local 랜덤 엔진
Vector3 AgentComponent::PickRandomTarget() {
    thread_local std::mt19937 gen(std::random_device{}());
    std::uniform_real_distribution<float> dist(-range, range);
    return Vector3(dist(gen), 0, dist(gen));
}
```

---

## 유형 B: TOCTOU (Time-of-Check-Time-of-Use)

### AliceEngine - 2.4

```cpp
// BAD - Lock 해제 후 캐시 데이터 접근
std::shared_ptr<Resource> ResourceManager::Load(const std::string& path) {
    {
        std::lock_guard<std::mutex> lock(m_cacheMutex);
        auto it = m_cache.find(path);
        if (it != m_cache.end()) {
            return it->second;  // 여기는 안전
        }
    }  // lock 해제!

    // 여기서 다른 스레드가 m_cache를 수정할 수 있음
    auto resource = LoadFromDisk(path);

    {
        std::lock_guard<std::mutex> lock(m_cacheMutex);
        m_cache[path] = resource;  // 다른 스레드가 이미 같은 키에 저장했을 수 있음
    }
    return resource;
}
```

### GOOD

```cpp
// GOOD - Lock 범위를 확장하거나 double-check locking
std::shared_ptr<Resource> ResourceManager::Load(const std::string& path) {
    std::lock_guard<std::mutex> lock(m_cacheMutex);

    auto it = m_cache.find(path);
    if (it != m_cache.end()) {
        return it->second;
    }

    // Lock 내에서 로드 (단순하지만 로딩이 긴 경우 블로킹)
    auto resource = LoadFromDisk(path);
    m_cache[path] = resource;
    return resource;
}

// GOOD - 또는 concurrent map 사용
// GOOD - 또는 promise/future 패턴으로 중복 로딩 방지
```

---

## 유형 C: 전역 상태 동기화 없음

### AliceEngine - 2.8

```cpp
// BAD - FMOD 전역 포인터에 동기화 없음
FMOD::System* g_System = nullptr;
FMOD::ChannelGroup* g_MasterGroup = nullptr;
std::map<std::wstring, SoundData> g_SoundBank;
std::vector<FMOD::Channel*> g_ChannelsSFX;
// 어떤 스레드에서든 접근 가능하지만 mutex가 없음
```

### MiKuEngine - M-04

```cpp
// BAD - 익명 네임스페이스의 글로벌 변수
namespace {
    std::unordered_map<BuffType, BuffInfo> g_activeBuffs;
    std::vector<std::function<void()>> g_pendingCallbacks;
    // 단일 스레드에서는 안전하지만 문서화되지 않음
}
```

### GOOD

```cpp
// GOOD - mutex 보호
class SoundManager {
    std::mutex m_mutex;
    FMOD::System* m_system = nullptr;
    std::map<std::wstring, SoundData> m_soundBank;

    void PlaySound(const std::wstring& name) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_soundBank.find(name);
        if (it != m_soundBank.end()) {
            // ...
        }
    }
};

// GOOD - 또는 단일 스레드 제약을 명시
// @thread_constraint: MainThread only
namespace {
    std::unordered_map<BuffType, BuffInfo> g_activeBuffs;
}
```

---

## 유형 D: Deadlock 위험

### AliceEngine - 1.2

```cpp
// BAD - 여러 mutex를 일관되지 않은 순서로 획득
// 코드 경로 A:
std::scoped_lock lock(s->eventMtx, s->contactStateMtx);  // 순서: event → contact

// 코드 경로 B (다른 함수):
std::scoped_lock lock(s->contactStateMtx, s->eventMtx);  // 순서: contact → event
// → Deadlock 가능!
// 스레드 1: eventMtx 획득 → contactStateMtx 대기
// 스레드 2: contactStateMtx 획득 → eventMtx 대기
// → 양쪽 모두 영원히 대기
```

### EGOSIS - HI-05

```cpp
// BAD - ContactModify 콜백 내에서 Scene Query 호출 시 내부 락 데드락
void OnContactModify(PxContactModifyPair* pairs, int count) {
    // PhysX 내부 락이 이미 걸려있는 상태
    physicsWorld->Raycast(origin, dir, maxDist, result);
    // Raycast도 PhysX 내부 락이 필요 → 데드락!
}
```

### GOOD

```cpp
// GOOD - 항상 같은 순서로 mutex 획득
// 전역 규칙: eventMtx를 항상 먼저 획득
std::scoped_lock lock(s->eventMtx, s->contactStateMtx);
// std::scoped_lock은 교착 회피 알고리즘을 사용하므로 안전

// GOOD - 또는 단일 mutex로 병합
std::mutex physicsMtx;  // eventMtx + contactStateMtx 통합

// GOOD - 콜백에서 Query를 지연 처리
void OnContactModify(PxContactModifyPair* pairs, int count) {
    // 콜백에서는 쿼리를 큐에 넣기만 함
    m_deferredQueries.push({origin, dir, maxDist});
}

void PostPhysicsStep() {
    // 물리 시뮬레이션 완료 후 지연 쿼리 실행
    for (auto& query : m_deferredQueries) {
        physicsWorld->Raycast(query.origin, query.dir, query.maxDist, result);
    }
    m_deferredQueries.clear();
}
```

---

## 유형 E: Resize 도중 렌더링 리소스 접근

### EGOSIS - HI-06

```cpp
// BAD - Resize 중간에 렌더링이 null 리소스 접근
void D3D11RenderDevice::Resize(uint32_t width, uint32_t height) {
    m_renderTargetView.Reset();  // RTV 해제 (null이 됨)
    m_depthStencilView.Reset();  // DSV 해제 (null이 됨)
    // ← 이 시점에서 WM_PAINT 메시지로 렌더링이 호출되면?
    m_swapChain->ResizeBuffers(...);
    CreateRenderTarget();        // RTV 재생성
    CreateDepthStencil();        // DSV 재생성
}

void D3D11RenderDevice::BeginFrame() {
    m_context->OMSetRenderTargets(1,
        m_renderTargetView.GetAddressOf(),  // null일 수 있음!
        m_depthStencilView.Get());          // null일 수 있음!
}
```

### GOOD

```cpp
// GOOD - Resize 중 렌더링 차단
void D3D11RenderDevice::Resize(uint32_t width, uint32_t height) {
    m_isResizing = true;  // 플래그로 렌더링 차단
    m_renderTargetView.Reset();
    m_depthStencilView.Reset();
    m_swapChain->ResizeBuffers(...);
    CreateRenderTarget();
    CreateDepthStencil();
    m_isResizing = false;
}

void D3D11RenderDevice::BeginFrame() {
    if (m_isResizing) return;  // Resize 중이면 렌더링 건너뛰기
    // ...
}
```

---

## 유형 F: 비동기 씬 로딩 경쟁 조건

### MiKuEngine - C-12

```cpp
// BAD - 워커 스레드에서 씬 데이터 수정, 메인 스레드에서 읽기
void SceneManager::ChangeScene(const std::string& sceneName) {
    LoadSceneResourceAsync(sceneName, [this](auto scene) {
        // 워커 스레드 콜백
        m_scene->Load();  // CreateGameObject() 호출 → m_incubator 수정
    });
}

// 동시에 메인 스레드에서:
void Scene::Update() {
    ProcessPendingAdds();  // m_incubator 읽기
    // → 데이터 레이스!
}
```

### GOOD

```cpp
// GOOD - 워커 스레드는 데이터만 준비, 메인 스레드에서 적용
void SceneManager::ChangeScene(const std::string& sceneName) {
    LoadSceneResourceAsync(sceneName, [this](auto sceneData) {
        // 워커 스레드: 리소스만 로드
        std::lock_guard<std::mutex> lock(m_pendingMutex);
        m_pendingSceneData = std::move(sceneData);
    });
}

void SceneManager::Update() {
    std::unique_ptr<SceneData> pending;
    {
        std::lock_guard<std::mutex> lock(m_pendingMutex);
        pending = std::move(m_pendingSceneData);
    }
    if (pending) {
        m_scene->Load(std::move(pending));  // 메인 스레드에서 적용
    }
}
```

---

## 발견 프로젝트별 분포

| 프로젝트 | Thread Safety 이슈 수 | 대표 사례 |
|----------|----------------------|----------|
| TigerEngine | ~4건 | static 버퍼, static 랜덤, rand() |
| Scoopy | 0건 | (단일 스레드) |
| SPEngine | 1건 | static what() 버퍼 |
| EGOSIS | ~4건 | NewGuid, FixedUpdate, ContactModify, Resize |
| AliceEngine | ~5건 | Deadlock, TOCTOU, FMOD 전역, Logger |
| MiKuEngine | 2건 | 비동기 씬, static stride |

---

## 핵심 교훈

1. **`static` 로컬 변수를 가변 공유 상태로 사용하지 마라** (`thread_local` 사용)
2. **Lock 범위를 데이터 사용 완료까지 유지하라** (TOCTOU 방지)
3. **여러 mutex는 항상 같은 순서로 획득하라** (`std::scoped_lock` 사용)
4. **콜백 내에서의 동기 호출을 지연(deferred) 처리하라** (데드락 방지)
5. **워커 스레드에서 메인 스레드 데이터를 직접 수정하지 마라**
6. **스레드 제약을 문서화하라** (`@thread_constraint: MainThread`)
