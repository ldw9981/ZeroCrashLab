# 10. Logic Error (논리 오류)

> **크래시보다는 잘못된 동작을 유발하지만, 축적되면 크래시로 이어질 수 있는 유형**

---

## 개요

논리 오류는 즉시 크래시하지 않지만 **잘못된 게임 동작**, **데이터 오염**, **리소스 중복 사용** 등을
유발하며, 궁극적으로 크래시의 원인이 됩니다.

---

## 유형 A: 값 복사 vs 참조 혼동

### TigerEngine (JesaSang) - E-10

```cpp
// BAD - auto가 값 복사를 만듦 (참조 아님)
void Scene::CheckDestroy() {
    for (auto& [name, _] : mappedGameObjects) {
        auto container = mappedGameObjects[name];  // 값 복사!
        for (auto it = container.begin(); it != container.end(); ) {
            if (shouldDestroy(*it)) {
                it = container.erase(it);  // 복사본만 수정됨
            } else {
                ++it;
            }
        }
        // 실제 mappedGameObjects[name]은 그대로!
        // → 삭제된 오브젝트가 맵에 영구 잔존
    }
}
```

### GOOD

```cpp
// GOOD - auto&로 참조
auto& container = mappedGameObjects[name];  // 참조!
// 또는 range-for에서 직접 참조
for (auto& [name, container] : mappedGameObjects) {
    // container는 참조
}
```

---

## 유형 B: if / else if 누락

### Scoopy - H01

```cpp
// BAD - else if가 아닌 if로 다중 풀에 중복 추가 가능
void EnemySpawner::EnemyDeath(ObjPtr<GameObject> obj) {
    if (obj->GetComponent<NormalEnemy>())      // 조건 1
        NormalEnemys.push(obj);
    if (obj->GetComponent<ArrowEnemy>())       // else if 아님! 별도 조건
        ArrowEnemys.push(obj);
    if (obj->GetComponent<ThiefEnemy>())       // else if 아님!
        ThiefEnemys.push(obj);
    // 하나의 적이 여러 타입 컴포넌트를 가지면?
    // → 여러 풀에 동시에 추가 → 동일 객체 중복 재사용 → 데이터 손상
}
```

### GOOD

```cpp
// GOOD - else if로 상호 배타적 분기
void EnemySpawner::EnemyDeath(ObjPtr<GameObject> obj) {
    if (obj->GetComponent<NormalEnemy>())
        NormalEnemys.push(obj);
    else if (obj->GetComponent<ArrowEnemy>())
        ArrowEnemys.push(obj);
    else if (obj->GetComponent<ThiefEnemy>())
        ThiefEnemys.push(obj);
    else
        LOG_WARNING("Unknown enemy type");
}
```

---

## 유형 C: Dangling Else (중괄호 누락)

### TigerEngine - W-67

```cpp
// BAD - 중괄호 누락으로 if/else 바인딩이 의도와 다름
if (conditionA)
    if (conditionB)
        DoSomethingAB();
else                        // 프로그래머 의도: conditionA의 else
    DoSomethingNotA();      // 실제: conditionB의 else!
```

**들여쓰기와 실제 동작이 다름:**
- 의도: `conditionA`가 false일 때 `DoSomethingNotA()` 호출
- 실제: `conditionA && !conditionB`일 때 `DoSomethingNotA()` 호출

### GOOD

```cpp
// GOOD - 항상 중괄호 사용
if (conditionA) {
    if (conditionB) {
        DoSomethingAB();
    }
} else {
    DoSomethingNotA();
}
```

---

## 유형 D: 변수명과 실제 의미 불일치

### EGOSIS - ME-02

```cpp
// BAD - 변수명이 의미와 반대
const bool hasCamera = GetComponents<CameraComponent>().empty();
// hasCamera == true → 카메라가 "없다"는 의미!
// ...
c.primary = !hasCamera;  // 카메라가 없을 때 primary = false ???
// → 첫 카메라 생성 시 primary가 false로 설정됨
// → 게임 시작 시 활성 카메라가 없음!
```

### GOOD

```cpp
// GOOD - 변수명이 의미를 정확히 반영
const bool isEmpty = GetComponents<CameraComponent>().empty();
c.primary = isEmpty;  // 카메라가 없으면 이것이 첫 번째 → primary
```

---

## 유형 E: 오브젝트 풀 상태 미초기화

### Scoopy - A03

```cpp
// BAD - 풀에서 꺼낸 적의 상태가 이전 생의 값 유지
void EnemySpawner::SpawnEnemy(EnemyType type) {
    auto obj = pool.pop();
    obj->SetActive(true);
    // buildingTarget → 이전 적이 공격하던 건물 참조 유지
    // HitByPlayer → 이전 적이 맞은 상태 유지
    // state → 이전 적의 FSM 상태 유지
    // HP는 리셋하지만 다른 멤버는?
}
```

### GOOD

```cpp
// GOOD - 풀에서 꺼낼 때 완전 초기화
void EnemySpawner::SpawnEnemy(EnemyType type) {
    auto obj = pool.pop();
    auto* enemy = obj->GetComponent<Enemy>();
    enemy->ResetState();  // 모든 멤버 초기화
    obj->SetActive(true);
}

void Enemy::ResetState() {
    HP = maxHP;
    buildingTarget = {};
    HitByPlayer = false;
    state = EnemyState::Idle;
    attackTimer = 0.0f;
    // ... 모든 게임플레이 상태 초기화
}
```

---

## 유형 F: 잘못된 변수 전달

### Scoopy - L02

```cpp
// BAD - bulletpos를 계산하고 pos를 전달
void Building::AutoAttack() {
    auto pos = GetTransform()->GetWorldPosition();
    auto bulletpos = pos;
    bulletpos.y = 1.f;  // 높이 조정

    auto obj = pool.pop();
    obj->GetComponent<SnowBullet>()->StartBullet(
        pos,          // ← bulletpos 대신 pos를 전달!
        bulletsize,
        bulletSpeed,
        enemyTarget
    );
    // → 총알이 지면(y=0)에서 발사됨 (건물 높이가 아닌)
}
```

### GOOD

```cpp
// GOOD - 올바른 변수 전달
obj->GetComponent<SnowBullet>()->StartBullet(
    bulletpos,    // 높이가 조정된 위치
    bulletsize,
    bulletSpeed,
    enemyTarget
);
```

---

## 유형 G: 미사용 기능 / 데드 코드

### Scoopy - H06

```cpp
// BAD - GiveDebuff()가 어디서도 호출되지 않음
class DebuffBuilding : public Building {
    void Update() override {
        // GiveDebuff() 호출이 없음!
        // → 디버프 건물이 실제로 아무 기능도 하지 않음
    }

    void GiveDebuff() {
        // 실제 디버프 로직이 있지만 호출되지 않음
        for (auto& enemy : nearbyEnemies) {
            enemy->ApplyDebuff(debuffType);
        }
    }
};
```

### GOOD

```cpp
// GOOD - Update에서 기능 호출
void DebuffBuilding::Update() override {
    Building::Update();
    GiveDebuff();  // 디버프 적용
}
```

---

## 유형 H: 씬 리로드 시 상태 미리셋

### MiKuEngine - M-05

```cpp
// BAD - 씬 전환 시 콜백에 stale 포인터 남음
namespace {
    std::vector<std::function<void()>> g_pendingCallbacks;
    // 콜백이 이전 씬의 this 포인터를 캡처하고 있을 수 있음
}

// 씬 전환 후 g_pendingCallbacks의 콜백을 실행하면
// 캡처된 this 포인터가 이미 파괴된 객체를 가리킴
```

### GOOD

```cpp
// GOOD - 씬 전환 시 명시적 정리
void BuffManager::OnSceneUnload() {
    g_pendingCallbacks.clear();  // 모든 콜백 제거
    g_activeBuffs.clear();       // 모든 버프 제거
}
```

---

## 유형 I: 성능이 크래시로 이어지는 경우

### Scoopy - L06

```cpp
// BAD - 매 프레임 O(n) 검색 × 다수 오브젝트
void Player::Update() {
    auto enemies = FindGameObjectsWithTag("Enemy");  // O(n)
    // 80+ 적 × (Player + Castle + 8 Buildings + 80 Enemies) = 수천 번/프레임
}
```

### EGOSIS - ME-04

```cpp
// BAD - 디버거 중단점 후 복귀 시 FixedUpdate 수백 번 호출
m_fixedAcc += deltaTime;  // deltaTime이 10초면?
while (m_fixedAcc >= m_fixedDt) {
    CallFixedUpdate(world, m_fixedDt);  // 0.02초 간격이면 500번 호출!
    m_fixedAcc -= m_fixedDt;
}
// → 게임 프리즈 → 응답 없음 다이얼로그 → 강제 종료
```

### MiKuEngine - M-11

```cpp
// BAD - 모든 트리거 쌍 O(n^2)
void CollisionSystem::CheckTriggerTriggerOverlaps() {
    for (auto& a : m_triggers) {
        for (auto& b : m_triggers) {  // O(n^2)!
            if (&a != &b) CheckOverlap(a, b);
        }
    }
    // 트리거 100개면 10,000번 체크 → 프레임 드롭 → 스파이크 → 응답 없음
}
```

### GOOD

```cpp
// GOOD - 캐시 + 제한
// FindGameObjectsWithTag를 프레임당 1회만 호출
auto& enemies = CacheService::GetEnemies();  // 프레임 시작 시 한 번 캐시

// GOOD - FixedUpdate 횟수 제한
m_fixedAcc += deltaTime;
m_fixedAcc = std::min(m_fixedAcc, m_fixedDt * kMaxSteps);  // 최대 10회
while (m_fixedAcc >= m_fixedDt) {
    CallFixedUpdate(world, m_fixedDt);
    m_fixedAcc -= m_fixedDt;
}

// GOOD - 공간 분할로 O(n^2) 회피
void CollisionSystem::CheckTriggerTriggerOverlaps() {
    m_broadPhase.Update(m_triggers);
    auto& pairs = m_broadPhase.GetOverlappingPairs();  // O(n log n)
    for (auto& [a, b] : pairs) {
        CheckOverlap(a, b);
    }
}
```

---

## 유형 J: #pragma once 누락

### TigerEngine - W-72

```cpp
// BAD - include guard가 없는 헤더
#pragma    // once 가 빠져있음!

class Game_Cutting {
    // ...
};
// 다중 include 시 재정의 오류 → 컴파일 실패
```

### GOOD

```cpp
#pragma once  // 올바른 include guard

class Game_Cutting {
    // ...
};
```

---

## 발견 프로젝트별 분포

| 프로젝트 | Logic Error 이슈 수 | 대표 사례 |
|----------|---------------------|----------|
| TigerEngine | ~8건 | 값 복사 버그, dangling else, #pragma, 인덱스 미갱신 |
| Scoopy | ~6건 | else if 누락, 변수 오전달, 성능 문제, 미사용 기능 |
| SPEngine | ~3건 | PBR 미구현, Transform parent 미적용 |
| EGOSIS | ~3건 | 변수명 불일치, FixedUpdate 폭주, CreateCamera 반전 |
| AliceEngine | ~2건 | D3D 초기화 상태 미전파 |
| MiKuEngine | ~4건 | 씬 리로드 stale 콜백, 중복 조건, O(n^2) 충돌 |

---

## 핵심 교훈

1. **`auto` 사용 시 값 복사 vs 참조를 명확히 구분하라** (`auto&` vs `auto`)
2. **`if`/`else if`/`else` 체인을 정확히 사용하라**
3. **중괄호를 항상 사용하라** (dangling else 방지)
4. **변수명이 실제 의미를 정확히 반영하는지 확인하라**
5. **오브젝트 풀에서 재사용 시 모든 상태를 초기화하라**
6. **씬 전환 시 모든 전역/캐시 상태를 정리하라**
7. **성능 문제도 크래시로 이어질 수 있다** (프리즈 → 응답 없음 → 강제 종료)
