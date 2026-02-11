# 03. Iterator Invalidation (반복자 무효화)

> **컬렉션을 순회하는 도중 컬렉션 자체를 수정하여 반복자가 무효화되는 유형**

---

## 개요

`std::vector`, `std::map`, `std::unordered_map` 등의 컨테이너를 `for` 루프로 순회하면서
내부에서 `push_back`, `erase`, `insert` 등을 호출하면 반복자가 무효화됩니다.
**100% Undefined Behavior**로, 크래시가 즉시 발생하거나 데이터가 손상됩니다.

---

## 유형 A: range-for 내부에서 push_back

### TigerEngine (JesaSang) - S-1 (코드베이스 내 가장 위험한 버그)

```cpp
// BAD - 외부 벡터를 순회하면서 내부에서 push_back
for (auto& e : scriptComps)                        // 외부 루프: scriptComps 순회
{
    for (auto& e : pending_scriptComponents)       // 내부 루프
    {
        scriptComps.push_back(e);                  // scriptComps에 추가!
        // → 벡터 재할당 발생 → 외부 루프의 모든 반복자/참조 무효화!
    }
    pending_scriptComponents.clear();
    e->OnFixedUpdate(dt);                          // 댕글링 참조 접근 → 크래시!
}
```

**왜 위험한가:**
- `push_back`이 벡터 재할당을 일으키면 기존 메모리가 해제됨
- 외부 `for (auto& e : scriptComps)`의 `begin()`/`end()` 반복자가 해제된 메모리를 가리킴
- `e`는 이미 존재하지 않는 메모리의 참조 → **100% UB**

### GOOD

```cpp
// GOOD - 루프 종료 후 pending 항목 추가
// 방법 1: 루프 밖에서 병합
for (auto& e : scriptComps) {
    e->OnFixedUpdate(dt);
}
// 루프 끝난 후 안전하게 추가
scriptComps.insert(scriptComps.end(),
    pending_scriptComponents.begin(),
    pending_scriptComponents.end());
pending_scriptComponents.clear();

// 방법 2: 인덱스 기반 순회 (size 변경에 안전)
for (size_t i = 0; i < scriptComps.size(); ++i) {
    // 주의: 이 방법은 새로 추가된 요소도 순회함
    scriptComps[i]->OnFixedUpdate(dt);
}
// pending은 별도 처리
```

---

## 유형 B: range-for 내부에서 erase/remove

### TigerEngine - S-5 (CameraSystem::Clear)

```cpp
// BAD - 순회 중 벡터 원소 제거
void CameraSystem::Clear() {
    for (auto& cam : registered)       // range-for로 순회
    {
        RemoveCamera(cam);              // registered에서 원소 제거!
        // → 반복자 무효화 → 크래시 또는 원소 건너뛰기
    }
}
```

### MiKuEngine - H-07 (GameObject 자식 순회)

```cpp
// BAD - 자식 리스트를 참조로 순회하면서 콜백이 부모-자식 관계 변경 가능
void GameObject::UpdateActiveInHierarchy() {
    for (auto& child : m_transform->GetChildren()) {  // const 참조가 아닌 참조로 순회
        child->UpdateActiveInHierarchy();
        // 이 안에서 OnEnable()/OnDisable() 콜백이 호출됨
        // 콜백에서 부모-자식 관계를 변경하면 GetChildren() 리스트 수정!
    }
}
```

### MiKuEngine - H-10 (충돌 콜백 중 컴포넌트 변경)

```cpp
// BAD - 콜백 중 컴포넌트 리스트 변경 가능
void CollisionSystem::NotifyScriptsCollision(GameObject* go, ...) {
    auto& components = go->GetComponents();  // const 참조로 순회
    for (auto& comp : components) {
        comp->OnCollisionEnter(other);
        // OnCollisionEnter 내에서 AddComponent 또는 RemoveComponent 호출 가능
        // → vector<unique_ptr> 수정 → 반복자 무효화!
    }
}
```

### GOOD

```cpp
// GOOD - 별도 벡터에 수집 후 처리
void CameraSystem::Clear() {
    auto toRemove = registered;  // 복사본 생성
    for (auto& cam : toRemove) {
        RemoveCamera(cam);       // 원본 수정해도 toRemove는 영향 없음
    }
}

// GOOD - 역순 순회 + 인덱스
void CameraSystem::Clear() {
    for (int i = static_cast<int>(registered.size()) - 1; i >= 0; --i) {
        RemoveCamera(registered[i]);
    }
}

// GOOD - 자식 리스트 복사 후 순회
void GameObject::UpdateActiveInHierarchy() {
    auto children = m_transform->GetChildren();  // 복사본
    for (auto& child : children) {
        child->UpdateActiveInHierarchy();
    }
}

// GOOD - 콜백 중 변경을 지연 처리
void CollisionSystem::NotifyScriptsCollision(GameObject* go, ...) {
    auto components = go->GetComponents();  // 복사본으로 순회
    for (auto& comp : components) {
        comp->OnCollisionEnter(other);
    }
    // 지연된 컴포넌트 추가/제거를 여기서 처리
    go->ProcessPendingComponentChanges();
}
```

---

## 유형 C: unordered_map 순회 중 다른 엔티티 수정

### EGOSIS - CR-02

```cpp
// BAD - 스크립트 Update에서 다른 엔티티의 스크립트 추가/제거 가능
for (auto it = allScripts.begin(); it != allScripts.end(); ++it) {
    auto& [entityId, scriptList] = *it;
    for (auto& comp : scriptList) {
        comp.instance->Update(deltaTime);
        // Update() 내에서 다른 엔티티에 AddScript() 호출 가능
        // → allScripts(unordered_map)에 새 항목 삽입
        // → rehash 발생 → 모든 반복자 무효화!
    }
}
```

### EGOSIS - HI-07 (DestroyEntity 재귀 중 맵 변경)

```cpp
// BAD - 재귀적 삭제 중 컨테이너 변경
void World::DestroyEntity(EntityId id) {
    std::vector<EntityId> children = GetChildren(id);
    for (EntityId child : children) {
        DestroyEntity(child);  // 재귀! m_scripts, m_engineStorages 변경됨
    }
    // 부모 삭제 로직이 이미 변경된 컨테이너에 접근
    m_scripts.erase(it);
    for (auto& [typeIndex, storage] : m_engineStorages) {
        storage->Remove(id);  // 자식 삭제 시 이미 변경됨
    }
}
```

### GOOD

```cpp
// GOOD - 엔티티 ID 스냅샷으로 순회
void ScriptSystem::CallUpdate(World& world, float dt) {
    // 순회 전 엔티티 ID 목록 스냅샷
    std::vector<EntityId> entityIds;
    entityIds.reserve(allScripts.size());
    for (auto& [id, _] : allScripts) {
        entityIds.push_back(id);
    }

    for (EntityId id : entityIds) {
        auto it = allScripts.find(id);
        if (it == allScripts.end()) continue;  // 이미 제거됨
        for (auto& comp : it->second) {
            comp.instance->Update(deltaTime);
        }
    }
}

// GOOD - 2-pass 삭제
void World::DestroyEntity(EntityId id) {
    // Pass 1: 삭제 대상 수집 (재귀적)
    std::vector<EntityId> toDestroy;
    CollectEntityAndChildren(id, toDestroy);

    // Pass 2: OnDestroy 콜백 (삭제 전)
    for (EntityId eid : toDestroy) {
        NotifyOnDestroy(eid);
    }

    // Pass 3: 실제 삭제 (역순으로 자식 먼저)
    for (auto it = toDestroy.rbegin(); it != toDestroy.rend(); ++it) {
        RemoveEntityData(*it);
    }
}
```

---

## 유형 D: 맵 복사본 수정 (실제 맵 미반영)

### TigerEngine - E-10 (Scene::CheckDestroy)

```cpp
// BAD - 맵 값을 복사본으로 받아서 복사본만 수정
void Scene::CheckDestroy() {
    for (auto& [name, _] : mappedGameObjects) {
        auto container = mappedGameObjects[name];  // 값 복사! (참조 아님)
        for (auto it = container.begin(); it != container.end(); ) {
            if (shouldDestroy(*it)) {
                it = container.erase(it);  // 로컬 복사본만 수정됨!
            } else {
                ++it;
            }
        }
        // 실제 mappedGameObjects[name]은 변경되지 않음
        // → 삭제된 오브젝트가 맵에 영구 잔존 → 댕글링 인덱스로 크래시
    }
}
```

### GOOD

```cpp
// GOOD - 참조로 받기
void Scene::CheckDestroy() {
    for (auto& [name, container] : mappedGameObjects) {  // 참조!
        for (auto it = container.begin(); it != container.end(); ) {
            if (shouldDestroy(*it)) {
                it = container.erase(it);  // 실제 맵의 컨테이너가 수정됨
            } else {
                ++it;
            }
        }
    }
}
```

---

## 유형 E: 콜백에서의 간접적 컬렉션 수정

### MiKuEngine - H-08 (OnDestroy 콜백에서 다른 오브젝트 Destroy)

```cpp
// BAD - 삭제 처리 중 콜백이 새로운 삭제를 트리거
void Scene::ProcessPendingKills() {
    for (auto& go : m_gameObjectKillList) {
        go->BroadcastOnDestroy();  // OnDestroy 콜백 호출
        // 콜백에서 다른 GameObject의 Destroy() 호출 가능
        // → m_gameObjectKillList에 새 항목 추가
        // → m_morgue.clear()에서 이미 해제된 객체를 콜백이 참조할 수 있음
    }
    m_morgue.clear();  // 모든 것 해제
}
```

### GOOD

```cpp
// GOOD - 삭제를 2단계로 분리
void Scene::ProcessPendingKills() {
    // Phase 1: 현재 kill 리스트를 로컬로 이동
    auto currentKills = std::move(m_gameObjectKillList);
    m_gameObjectKillList.clear();

    // Phase 2: 모든 OnDestroy 콜백 먼저 호출
    for (auto& go : currentKills) {
        go->BroadcastOnDestroy();
    }
    // 콜백에서 추가된 새 삭제 대상은 m_gameObjectKillList에 들어감
    // → 다음 프레임에 처리

    // Phase 3: 실제 해제
    currentKills.clear();
}
```

---

## 발견 프로젝트별 분포

| 프로젝트 | Iterator Invalidation 이슈 수 | 대표 사례 |
|----------|-------------------------------|----------|
| TigerEngine | 4건 | ScriptSystem push_back, CameraSystem Clear, Scene 맵 복사 |
| Scoopy | 1건 | EnemyDeath 중 pool 수정 |
| EGOSIS | 3건 | Script Update 중 map 수정, DestroyEntity 재귀 |
| AliceEngine | 1건 | Deadlock (mutex 관련) |
| MiKuEngine | 3건 | 자식 순회 중 변경, 충돌 콜백, OnDestroy 콜백 |

---

## 핵심 교훈

1. **range-for 루프 내에서 순회 중인 컬렉션을 절대 수정하지 마라**
2. **수정이 필요하면 복사본으로 순회하거나, 인덱스 기반으로 순회하라**
3. **콜백/이벤트 내에서의 간접적 수정을 항상 경계하라**
4. **Deferred Queue 패턴: 변경사항을 큐에 넣고 루프 종료 후 일괄 처리하라**
5. **`auto container = map[key]`는 값 복사! `auto& container = map[key]`로 참조하라**
