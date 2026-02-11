# 09. Missing Return Value / Undefined Behavior (반환값 누락 / 정의되지 않은 동작)

> **함수가 값을 반환해야 하는데 특정 경로에서 return이 없거나, C++ 표준이 보장하지 않는 동작에 의존하는 유형**

---

## 개요

C++에서 non-void 함수가 return 없이 끝나면 **Undefined Behavior**입니다.
반환값은 스택의 가비지 값이 되어 **가비지 포인터 역참조**, **잘못된 분기**, **크래시** 등을 유발합니다.

---

## 유형 A: 함수의 모든 경로에 return이 없음

### TigerEngine (JesaSang) - E-2

```cpp
// BAD - 반환값 없이 함수 종료 가능
GameObject* Editor::InstantiatePrefabFromJson(const json& data) {
    std::vector<GameObject*> created;
    std::vector<int> parentIDs;

    // ... 프리팹 인스턴스화 로직 ...

    for (int i = 0; i < (int)created.size(); ++i) {
        if (created[i] && parentIDs[i] == -1) {
            return created[i];    // 루트 오브젝트 반환
        }
    }
    // ← 여기에 도달하면 return이 없음!
    // → 반환값은 가비지 포인터 → 호출자가 역참조하면 크래시!
}
```

### TigerEngine - E-3

```cpp
// BAD - PrefabUtil도 동일 패턴
GameObject* PrefabUtil::Instantiate(const json& prefabData) {
    // ... 생성 로직 ...
    for (int i = 0; i < (int)created.size(); ++i) {
        if (created[i] && parentIDs[i] == -1)
            return created[i];
    }
    // return 없음! → UB
}
```

### TigerEngine - E-4

```cpp
// BAD - Scene::GetGameObject도 동일
GameObject* Scene::GetGameObject(const std::string& name, int index) {
    auto it = mappedGameObjects.find(name);
    if (it != mappedGameObjects.end()) {
        auto& container = it->second;
        if (index < container.size()) {
            return gameObjects[container[index]].get();
        }
    }
    // return 없음! → 이름이 없거나 인덱스가 범위 밖이면 UB
}
```

### GOOD

```cpp
// GOOD - 모든 경로에 return 보장
GameObject* Editor::InstantiatePrefabFromJson(const json& data) {
    std::vector<GameObject*> created;
    std::vector<int> parentIDs;

    // ... 프리팹 인스턴스화 로직 ...

    for (int i = 0; i < (int)created.size(); ++i) {
        if (created[i] && parentIDs[i] == -1) {
            return created[i];
        }
    }

    LOG_WARNING("InstantiatePrefabFromJson: No root object found");
    return nullptr;  // 명시적 실패 반환
}

// GOOD - optional 반환
std::optional<GameObject*> Scene::GetGameObject(const std::string& name, int index) {
    auto it = mappedGameObjects.find(name);
    if (it != mappedGameObjects.end()) {
        auto& container = it->second;
        if (index < container.size()) {
            return gameObjects[container[index]].get();
        }
    }
    return std::nullopt;  // 명시적 "없음"
}
```

---

## 유형 B: 비교 연산자를 대입 대신 사용 (No-op)

### Scoopy - H08

```cpp
// BAD - == 를 = 대신 사용 (비교 결과를 버리는 no-op)
void Player::HandleAttack(float dt) {
    attackTimer == 0.0f;   // 대입이 아닌 비교! 아무 효과 없음!
    // attackTimer는 변경되지 않음
    // → 다음 공격 시 타이머가 리셋되지 않아 즉시 공격 발동
}
```

### GOOD

```cpp
// GOOD - = 대입 연산자 사용
void Player::HandleAttack(float dt) {
    attackTimer = 0.0f;    // 올바른 대입
}

// 방어: 컴파일러 경고 활성화
// MSVC: /W4 → "expression has no effect" 경고
// GCC/Clang: -Wunused-value
```

---

## 유형 C: 역참조 후 null 검사 (순서 오류)

### TigerEngine - S-10, M-5

```cpp
// BAD - 역참조를 먼저 하고 null 검사를 나중에
void PhysicsSystem::Update() {
    auto* actor = hit.actor;
    auto name = actor->getName();   // actor가 null이면 여기서 크래시!

    if (actor == nullptr) {         // 데드 코드! 위에서 이미 크래시
        return;
    }
}
```

```cpp
// BAD - pScene 접근 후 null 검사
auto transform = pScene->mRootNode->mTransformation;  // pScene이 null이면 크래시!

if (pScene == nullptr) {  // 데드 코드
    return nullptr;
}
```

### GOOD

```cpp
// GOOD - null 검사를 먼저
void PhysicsSystem::Update() {
    auto* actor = hit.actor;
    if (!actor) return;             // 먼저 검사!
    auto name = actor->getName();   // 안전
}

// GOOD
if (!pScene) return nullptr;
auto transform = pScene->mRootNode->mTransformation;
```

---

## 유형 D: HRESULT 체크 없이 후속 사용

### SPEngine (Scoopy 4Q) - H-04

```cpp
// BAD - HR_T 예외를 catch하는 코드가 전체 프로젝트에 없음
void SomeFunction() {
    HR_T(device->CreateBuffer(&desc, &data, &buffer));
    // HR_T는 실패 시 com_exception을 throw
    // 하지만 try-catch가 어디에도 없음!
    // → 셰이더 컴파일 실패, 버퍼 생성 실패 등 어떤 D3D 작업이든
    //   실패하면 처리되지 않은 예외로 프로그램 즉시 종료
}
```

### AliceEngine - 3.6

```cpp
// BAD - 부분 초기화 후 실패 시 무효 상태
void DeferredRenderSystem::InitGBuffer() {
    if (FAILED(m_device->CreateTexture2D(&desc1, nullptr, &tex1))) return;
    // tex1 성공, tex2 실패 시 GBuffer가 부분적으로만 유효
    if (FAILED(m_device->CreateTexture2D(&desc2, nullptr, &tex2))) return;
    if (FAILED(m_device->CreateRenderTargetView(tex1, nullptr, &rtv1))) return;
    // → 호출자는 초기화 실패를 감지할 방법이 없음 (void 반환)
    // → 부분 초기화된 GBuffer로 렌더링 시도 → 크래시
}
```

### GOOD

```cpp
// GOOD - 최소한 main에 try-catch
int WINAPI WinMain(...) {
    try {
        app.Initialize();
        app.Run();
    } catch (const com_exception& e) {
        MessageBoxA(nullptr, e.what(), "DirectX Error", MB_ICONERROR);
        return -1;
    } catch (const std::exception& e) {
        MessageBoxA(nullptr, e.what(), "Error", MB_ICONERROR);
        return -1;
    }
    return 0;
}

// GOOD - 초기화 성공/실패 반환
bool DeferredRenderSystem::InitGBuffer() {
    if (FAILED(m_device->CreateTexture2D(&desc1, nullptr, &tex1))) return false;
    if (FAILED(m_device->CreateTexture2D(&desc2, nullptr, &tex2))) {
        tex1->Release();  // 정리
        return false;
    }
    return true;
}
```

---

## 유형 E: 무한 재귀 / 스택 오버플로

### EGOSIS - CR-04

```cpp
// BAD - 순환 참조 시 무한 재귀
void World::MarkTransformDirty(EntityId entityId) {
    m_transformDirty[entityId] = true;
    const auto& transforms = GetComponents<TransformComponent>();
    for (const auto& [eid, tr] : transforms) {
        if (tr.parent == entityId) {
            MarkTransformDirty(eid);  // 재귀! 순환 시 무한 루프 → 스택 오버플로
        }
    }
}
```

### AliceEngine - 3.1

```cpp
// BAD - FBX 계층 구조 재귀에 깊이 제한 없음
void BuildNodeHierarchy(aiNode* node, int parent) {
    // ...
    for (unsigned i = 0; i < node->mNumChildren; ++i) {
        BuildNodeHierarchy(node->mChildren[i], currentIdx);  // 재귀
        // 비정상 FBX: 깊이 100+이면 스택 오버플로
    }
}
```

### MiKuEngine - L-04

```cpp
// BAD - Scene Save 시 순환 참조 → 무한 재귀
std::function<void(Transform*)> traverse;
traverse = [&](Transform* t) {
    // ... serialize
    for (auto* child : t->GetChildren()) {
        traverse(child);  // 순환 참조가 있으면 무한 재귀
    }
};
```

### GOOD

```cpp
// GOOD - visited set으로 순환 감지
void World::MarkTransformDirty(EntityId entityId) {
    std::unordered_set<EntityId> visited;
    MarkTransformDirtyImpl(entityId, visited);
}

void World::MarkTransformDirtyImpl(EntityId entityId,
                                    std::unordered_set<EntityId>& visited) {
    if (!visited.insert(entityId).second) return;  // 이미 방문 → 순환!
    m_transformDirty[entityId] = true;
    for (const auto& [eid, tr] : GetComponents<TransformComponent>()) {
        if (tr.parent == entityId) {
            MarkTransformDirtyImpl(eid, visited);
        }
    }
}

// GOOD - 깊이 제한
void BuildNodeHierarchy(aiNode* node, int parent, int depth = 0) {
    if (depth > 64) {
        LOG_WARNING("FBX hierarchy too deep (>64), stopping");
        return;
    }
    // ...
    for (unsigned i = 0; i < node->mNumChildren; ++i) {
        BuildNodeHierarchy(node->mChildren[i], currentIdx, depth + 1);
    }
}
```

---

## 유형 F: 타입 불일치

### TigerEngine - B-18 / SPEngine (Scoopy 4Q) - H-08

```cpp
// BAD - char 배열을 wchar_t 함수에 전달
#define LOG_WARNINGA(fmt, ...) \
    char buffer[256]; \
    sprintf_s(buffer, 256, fmt, __VA_ARGS__); \
    OutputDebugStringW(buffer);  // W 함수에 char* 전달!
    // char 데이터를 wchar_t로 해석 → 가비지 출력 또는 경계 밖 읽기
```

### GOOD

```cpp
// GOOD - 타입 일치
#define LOG_WARNINGA(fmt, ...) \
    char buffer[256]; \
    sprintf_s(buffer, 256, fmt, __VA_ARGS__); \
    OutputDebugStringA(buffer);  // A 함수에 char* 전달!
```

---

## 발견 프로젝트별 분포

| 프로젝트 | UB 이슈 수 | 대표 사례 |
|----------|-----------|----------|
| TigerEngine | ~8건 | Missing return, 역참조 후 null 검사, 타입 불일치 |
| Scoopy | 1건 | == vs = 오타 |
| SPEngine | ~3건 | 예외 미처리, 타입 불일치 |
| EGOSIS | 2건 | 무한 재귀, off-by-one 문자열 비교 |
| AliceEngine | ~3건 | 부분 초기화, 무한 재귀 |
| MiKuEngine | 1건 | 씬 저장 순환 참조 |

---

## 핵심 교훈

1. **non-void 함수는 모든 경로에서 return이 있어야 한다** (컴파일러 경고 `/W4` 활성화)
2. **null 검사는 역참조 전에 하라** (역참조 후 검사는 데드 코드)
3. **`==`와 `=`를 혼동하지 마라** (컴파일러 경고 활성화)
4. **예외를 throw하면 반드시 catch하는 코드가 있어야 한다**
5. **재귀 함수에는 깊이 제한 또는 순환 감지를 추가하라**
6. **A/W 함수 버전과 char/wchar_t 타입을 일치시켜라**
