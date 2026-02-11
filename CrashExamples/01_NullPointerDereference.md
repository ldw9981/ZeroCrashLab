# 01. Null Pointer Dereference (널 포인터 역참조)

> **6개 프로젝트 전체에서 가장 빈번하게 발견된 크래시 유형 (전체 이슈의 약 40%)**

---

## 개요

포인터가 `nullptr`인 상태에서 `->` 또는 `*` 연산자로 접근하면 **Access Violation (0xC0000005)** 으로 즉시 크래시합니다.

---

## 유형 A: Find/Get 반환값 미검사 체인

가장 지배적인 패턴. `Find()`, `GetComponent<T>()` 등의 반환값이 null일 수 있는데, 검사 없이 바로 체인 호출합니다.

### TigerEngine (JesaSang) - W-1~W-43, 45건

```cpp
// BAD - 이름으로 검색 후 즉시 역참조 (40+ 위치에서 반복)
camController = CameraSystem::Instance().GetCurrCamera()
    ->GetOwner()->GetComponent<CameraController>();
// GetCurrCamera()가 null이면? GetOwner()가 null이면? GetComponent가 null이면?
// -> 어느 단계에서든 즉시 크래시
```

```cpp
// BAD - 대표적 패턴
auto obj = SceneUtil::GetObjectByName("UI_MiniMap_Controller");
obj->GetComponent<MiniMapController>()->SetActive(true);  // obj가 null이면 크래시
```

### Scoopy - C02, C07

```cpp
// BAD - Find 결과를 바로 역참조
player = GetGameObject()->Find("Player");
castle = GetGameObject()->Find("Castle");
playercomp = player->GetComponent<Player>();  // player가 null이면 크래시
castlecomp = castle->GetComponent<Castle>();  // castle이 null이면 크래시
```

```cpp
// BAD - FindWithTag 후 즉시 체인
mPlayer = GameObject::FindWithTag("Player")->GetComponent<Player>();
// "Player" 태그가 없으면 즉시 크래시
```

### MiKuEngine - C-01~C-07

```cpp
// BAD - Prefab::Instantiate 결과 미검사
void BulletFactory::Fire(Vector3 position, Vector3 direction) {
    auto go = Prefab::Instantiate("BulletPlayer");
    go->GetTransform()->SetLocalPosition(position);  // go가 null이면 크래시
    auto bullet = go->GetComponent<BulletPlayer>();
    bullet->SetDirection(direction);                  // bullet이 null이면 크래시
}
```

```cpp
// BAD - Boss HP UI 찾기 실패
auto go = GameObject::Find("UI_BossHP");
m_hpText = go->GetComponent<UIText>();  // go가 null이면 크래시
```

### EGOSIS - CR-05

```cpp
// BAD - SessionState의 fighterMap에서 조회
Fighter* fighter = sessionState.fighterMap[entityId];
fighter->ApplyDamage(damage);  // entityId가 맵에 없으면 nullptr 크래시
```

---

### GOOD - 올바른 수정 방법

```cpp
// GOOD - 단계별 null 검사
auto* cam = CameraSystem::Instance().GetCurrCamera();
if (!cam) return;

auto* owner = cam->GetOwner();
if (!owner) return;

auto* controller = owner->GetComponent<CameraController>();
if (!controller) return;

camController = controller;
```

```cpp
// GOOD - 헬퍼 함수로 안전한 접근
template<typename T>
T* SafeGetComponent(const std::string& objectName) {
    auto* obj = SceneUtil::GetObjectByName(objectName);
    if (!obj) {
        LOG_WARNING("Object '%s' not found", objectName.c_str());
        return nullptr;
    }
    return obj->GetComponent<T>();
}

// 사용
auto* controller = SafeGetComponent<MiniMapController>("UI_MiniMap_Controller");
if (controller) {
    controller->SetActive(true);
}
```

---

## 유형 B: 싱글턴 인스턴스 미검사

### Scoopy - C01, C04, C06

```cpp
// BAD - 싱글턴 인스턴스를 검사 없이 역참조
EnemySpawner::instance->WaveSetting(wave);       // instance가 null이면 크래시
BattleManager::instance->Attack(attacker, target); // instance가 null이면 크래시
GameManager::instance->nowSetting;                 // instance가 null이면 크래시
```

### GOOD

```cpp
// GOOD - 싱글턴 접근 전 유효성 검사
if (auto* spawner = EnemySpawner::instance) {
    spawner->WaveSetting(wave);
} else {
    LOG_ERROR("EnemySpawner not initialized");
}
```

---

## 유형 C: 엔진 시스템 내부 null 미검사

### TigerEngine - S-2 (SceneSystem)

```cpp
// BAD - currentScene이 null인데 검사 없음
void SceneSystem::BeforUpdate() {
    if (scenes.empty()) return;
    currentScene->CheckDestroy();   // currentScene이 null이면 크래시!
    // scenes가 비어있지 않아도 currentScene은 null일 수 있음
}
```

### TigerEngine - C-1~C-3 (Camera)

```cpp
// BAD - owner의 Transform null 미검사
Vector3 Camera::GetForward() {
    return owner->GetTransform()->GetForward();  // owner나 GetTransform()이 null이면 크래시
}
```

### SPEngine (Scoopy 4Q) - C-04

```cpp
// BAD - 카메라 포인터 미검사
void RenderPipe::Render() {
    m_camMat.camPos = (Vector4)m_renderCam->m_Position;  // m_renderCam이 null이면 크래시
    m_renderCam->GetViewMatrix(m_camMat.mView);
}
```

### GOOD

```cpp
// GOOD
void SceneSystem::BeforUpdate() {
    if (scenes.empty() || !currentScene) return;
    currentScene->CheckDestroy();
}

// GOOD
Vector3 Camera::GetForward() {
    if (!owner) return Vector3::Forward;
    auto* transform = owner->GetTransform();
    if (!transform) return Vector3::Forward;
    return transform->GetForward();
}
```

---

## 유형 D: weak_ptr::lock() 미검증

### SPEngine (Scoopy 4Q) - C-05

```cpp
// BAD - lock() 반환값 검사 없이 -> 연산자 사용
auto zelda = ObjectManager::GetInstance()->CreateObject();
auto zmc = zelda.lock()->AddComponent<MeshRenderer>();    // lock()이 nullptr 반환 가능!
zmc.lock()->SetMesh(zmesh);                                // 이중 lock 체인 매우 위험
zelda.lock()->transform.lock()->Position = Vector3(0, 0, 0); // 다중 lock 체인
```

### AliceEngine - 3.8

```cpp
// BAD - weak_ptr lock 결과 미검증
std::weak_ptr<Impl> owner;
auto locked = owner.lock();
locked->DoSomething();  // locked가 nullptr이면 크래시
```

### GOOD

```cpp
// GOOD - lock 후 반드시 검사
if (auto obj = zelda.lock()) {
    if (auto mc = obj->AddComponent<MeshRenderer>().lock()) {
        mc->SetMesh(zmesh);
    }
    if (auto tr = obj->transform.lock()) {
        tr->Position = Vector3(0, 0, 0);
    }
}
```

---

## 유형 E: 콜백/이벤트 시점의 null

### MiKuEngine - C-04

```cpp
// BAD - CacheComponents에서 찾지 못한 포인터를 매 프레임 사용
void PlayerControllerScript::UpdateGameLogic() {
    Vector3 dir = m_aimPointer->GetDirectionFrom(playerPos);
    // m_aimPointer가 CacheComponents()에서 null로 남아있으면 매 프레임 크래시
}
```

### GOOD

```cpp
// GOOD
void PlayerControllerScript::UpdateGameLogic() {
    if (!m_aimPointer) return;
    Vector3 dir = m_aimPointer->GetDirectionFrom(playerPos);
}
```

---

## 발견 프로젝트별 분포

| 프로젝트 | Null Deref 이슈 수 | 비율 |
|----------|-------------------|------|
| TigerEngine | ~95건 | 43% |
| Scoopy | ~14건 | 40% |
| SPEngine | ~5건 | 19% |
| EGOSIS | ~8건 | 25% |
| AliceEngine | ~7건 | 28% |
| MiKuEngine | ~18건 | 38% |

---

## 핵심 교훈

1. **반환값이 null일 수 있는 모든 함수 호출 후 검사하라**
2. **체인 호출(`a->b()->c()->d()`)을 피하고 단계별로 검사하라**
3. **싱글턴 접근은 항상 유효성을 먼저 확인하라**
4. **`weak_ptr::lock()` 결과는 반드시 검사하라**
5. **SafeGet 헬퍼 패턴을 도입하여 프로젝트 전체에 일관되게 적용하라**
