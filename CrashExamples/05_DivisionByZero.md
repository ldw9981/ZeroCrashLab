# 05. Division by Zero / INF / NaN (0 나누기 / 무한대 / NaN)

> **수학 연산에서 0으로 나누거나, 결과가 무한대/NaN이 되어 후속 연산을 오염시키는 유형**

---

## 개요

- **정수 나누기 0**: 즉시 하드웨어 예외 → 크래시
- **부동소수점 나누기 0**: `inf` 또는 `NaN` 생성 → 후속 물리/렌더링에서 오브젝트 소실, 텔레포트, 또는 최종 크래시
- **기본값 0으로 초기화된 변수**가 분모로 사용되는 패턴이 가장 빈번

---

## 유형 A: 기본값 0인 멤버 변수가 분모

### TigerEngine (JesaSang) - R-1

```cpp
// BAD - fps 기본값이 0.0f인데 나누기에 사용
class SpriteSheet {
    float fps = 0.0f;       // 기본값 0!
    int frameCount = 1;

    float GetFilpbookDuration() {
        return frameCount / fps;   // fps == 0.0f → Division by Zero!
        // 부동소수점이므로 inf 반환 → 파티클 시스템 오동작
    }
};
```

### TigerEngine - C-4 (Camera)

```cpp
// BAD - screenHeight 기본값 0
class Camera {
    int screenWidth = 0;
    int screenHeight = 0;  // 기본값 0!

    void SetProjection() {
        float aspect = (float)screenWidth / (float)screenHeight;
        // screenHeight == 0 → Division by Zero → inf aspect ratio
        // → 프로젝션 행렬이 inf → 모든 렌더링 깨짐
    }
};
```

### EGOSIS - CR-01

```cpp
// BAD - 창 최소화 시 m_height가 0이 될 수 있음
const float defaultAspect = static_cast<float>(m_width) / m_height;
// m_height == 0 → Division by Zero
```

### GOOD

```cpp
// GOOD - 분모가 0이 아닌지 검사
float GetFilpbookDuration() {
    if (fps <= 0.0f) return 0.0f;  // 또는 기본 duration 반환
    return frameCount / fps;
}

void SetProjection() {
    if (screenHeight <= 0) return;
    float aspect = (float)screenWidth / (float)screenHeight;
    // ...
}

// GOOD - std::max로 최소값 보장
const float safeHeight = std::max(1u, m_height);
const float defaultAspect = static_cast<float>(m_width) / safeHeight;
```

---

## 유형 B: 거리 계산에서 매우 작은 값

### Scoopy - H07

```cpp
// BAD - 매우 가까운 거리에서 역수 계산
void Enemy::CheckPlayer() {
    float dx = playerPos.x - enemyPos.x;
    float dz = playerPos.z - enemyPos.z;
    float pd2 = dx * dx + dz * dz;

    float inv = 1.0f / sqrtf(pd2);
    // pd2가 매우 작으면 sqrtf(pd2) ≈ 0 → inv ≈ infinity
    // → 방향 벡터가 inf → 적이 순간이동
}
```

### MiKuEngine - M-01

```cpp
// BAD - 공격 속도가 0이면 무한대 발사 간격
float effectiveFireRate = 0.7f / effectiveAtkSpeed;
// effectiveAtkSpeed == 0 → infinity → 영원히 발사 불가
```

### GOOD

```cpp
// GOOD - 최소값 보장
void Enemy::CheckPlayer() {
    float pd2 = dx * dx + dz * dz;
    if (pd2 < 0.0001f) {
        // 너무 가까우면 기본 방향 사용
        direction = Vector3::Forward;
        return;
    }
    float inv = 1.0f / sqrtf(pd2);
}

// GOOD - 클램핑
float effectiveAtkSpeed = std::max(0.001f, atkSpeed * modifier);
float effectiveFireRate = 0.7f / effectiveAtkSpeed;
```

---

## 유형 C: 에디터/외부 데이터에서 온 분모

### TigerEngine - E-9

```cpp
// BAD - 에디터에서 화면 크기를 0으로 설정할 수 있음
void Editor::OnInputProcess() {
    float ndcX = (2.0f * mouseX) / screenWidth - 1.0f;
    float ndcY = 1.0f - (2.0f * mouseY) / screenHeight;
    // 에디터 뷰포트 크기가 0이면 Division by Zero
}
```

### TigerEngine - B-16 (NodeAnimation)

```cpp
// BAD - perTick이 외부 데이터에서 로드되며 0일 수 있음
for (int i = 0; i < pAiNodeAnim->mNumPositionKeys; i++) {
    float time = pAiNodeAnim->mPositionKeys[i].mTime / perTick;
    // FBX에서 로드한 perTick이 0이면 모든 키 시간이 inf/NaN
}
```

### MiKuEngine - M-12

```cpp
// BAD - 해상도가 매우 작을 때
float texelSizeX = 1.0f / (m_resolutionWidth / 2.0f);
// m_resolutionWidth == 0 → 0으로 나누기
// m_resolutionWidth == 1 → 1.0f / 0.5f = 2.0f (과도한 blur)
```

### GOOD

```cpp
// GOOD - 외부 데이터 검증
void Editor::OnInputProcess() {
    if (screenWidth <= 0 || screenHeight <= 0) return;
    float ndcX = (2.0f * mouseX) / screenWidth - 1.0f;
    // ...
}

// GOOD - 로드된 데이터 검증
float safeTick = (perTick > 0.0f) ? perTick : 1.0f;
for (int i = 0; i < numKeys; i++) {
    float time = keys[i].mTime / safeTick;
}

// GOOD - 최소 해상도 보장
uint32_t safeWidth = std::max(2u, m_resolutionWidth);
float texelSizeX = 1.0f / (safeWidth / 2.0f);
```

---

## 유형 D: NaN 전파 (영벡터 정규화)

### SPEngine (Scoopy 4Q) - L-03

```cpp
// BAD - 영벡터 정규화 시 NaN
void Camera::AddInputVector(const Vector3& input) {
    m_InputVector += input;
    m_InputVector.Normalize();
    // m_InputVector가 (0,0,0)에 가까우면 Normalize()가 NaN 생성
    // → Position += m_InputVector * speed → Position이 NaN
    // → 카메라 위치가 NaN → 모든 렌더링 실패
}
```

### MiKuEngine - M-16

```cpp
// BAD - 이동 방향이 영벡터일 때 정규화
m_objectLogicalForward.Normalize();
// 정지 상태에서 (0,0,0) → Normalize → NaN
// → 이후 모든 위치/방향 계산에 NaN 전파
```

### GOOD

```cpp
// GOOD - 정규화 전 길이 검사
void Camera::AddInputVector(const Vector3& input) {
    m_InputVector += input;
    float lengthSq = m_InputVector.LengthSquared();
    if (lengthSq > 1e-8f) {
        m_InputVector /= sqrtf(lengthSq);  // 수동 정규화
    } else {
        m_InputVector = Vector3::Zero;      // 영벡터 유지
    }
}

// GOOD - SafeNormalize 헬퍼
Vector3 SafeNormalize(const Vector3& v, const Vector3& fallback = Vector3::Forward) {
    float lenSq = v.LengthSquared();
    if (lenSq < 1e-8f) return fallback;
    return v / sqrtf(lenSq);
}
```

---

## 유형 E: 파티클/이펙트 0 나누기

### TigerEngine - R-30

```cpp
// BAD - 파티클 수명이 0일 때 alpha fade 계산
float alpha = age / lifetime;
// lifetime == 0 → alpha == inf
// → 파티클 색상이 inf → GPU 렌더링 오류
```

### GOOD

```cpp
// GOOD
float alpha = (lifetime > 0.001f) ? (age / lifetime) : 1.0f;
alpha = std::clamp(alpha, 0.0f, 1.0f);
```

---

## 발견 프로젝트별 분포

| 프로젝트 | Division by Zero 이슈 수 | 대표 사례 |
|----------|--------------------------|----------|
| TigerEngine | ~6건 | SpriteSheet fps, Camera aspect, NodeAnimation perTick |
| Scoopy | 1건 | Enemy 거리 역수 계산 |
| SPEngine | 1건 | Camera 영벡터 정규화 |
| EGOSIS | 1건 | Camera aspect (창 최소화) |
| AliceEngine | 0건 | - |
| MiKuEngine | 2건 | 공격 속도, Bloom 해상도 |

---

## 핵심 교훈

1. **나누기 전에 분모가 0이 아닌지 항상 검사하라**
2. **기본값이 0인 멤버 변수를 분모로 사용하지 마라 (최소 1로 초기화)**
3. **외부 데이터(FBX, JSON, 에디터)에서 온 값은 반드시 검증하라**
4. **`Normalize()` 전에 벡터 길이를 확인하라 (SafeNormalize 패턴)**
5. **`inf`/`NaN`은 전파된다 - 하나의 나누기 오류가 전체 시스템을 오염시킨다**
6. **`std::clamp`, `std::max`로 안전 범위를 보장하라**
