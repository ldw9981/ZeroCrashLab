# 06. Uninitialized Variable (미초기화 변수)

> **선언만 하고 초기화하지 않은 변수에 접근하여 가비지 값으로 동작하거나 크래시하는 유형**

---

## 개요

C++에서 지역 변수와 POD 멤버 변수는 **자동으로 초기화되지 않습니다**.
미초기화 포인터는 가비지 주소를 가리켜 역참조 시 크래시하고,
미초기화 숫자는 잘못된 계산/분기를 유발합니다.

---

## 유형 A: 미초기화 포인터 멤버

### TigerEngine (JesaSang) - E-5, W-50~W-63

```cpp
// BAD - 포인터 멤버를 초기화하지 않음
class Editor {
    GameObject* selectedObject;  // 가비지 주소! nullptr이 아님!

    void DrawInspector() {
        if (selectedObject) {       // 가비지 주소는 0이 아니므로 true
            selectedObject->GetName();  // 가비지 주소 역참조 → 크래시!
        }
    }
};
```

```cpp
// BAD - FSM 상태 배열 미초기화
class PlayerController {
    IState* curState;         // 가비지 포인터!
    IState* fsmStates[9];     // 9개 모두 가비지 포인터!

    void Update() {
        curState->Execute();  // Init() 호출 전에 Update가 먼저 오면? → 크래시
    }
};
```

```cpp
// BAD - 여러 포인터 미초기화
class MiniGameManager {
    TextUI* mTimerUI;          // 가비지
    TextUI* mScoreUI;          // 가비지
    Image*  mGaugeBar;         // 가비지
    Image*  mGaugeFrame;       // 가비지
    Image*  mBackground;       // 가비지
    Image*  mResultUI;         // 가비지
    // Start()에서 Find로 초기화하지만, Start 실패 시 모두 가비지
};
```

### TigerEngine - W-58, W-60, W-61

```cpp
// BAD
class PauseManager {
    GameObject* pausePannel;    // 가비지!
};

class AITarget {
    Transform* tr;              // 가비지!
    Transform* playerTr;        // 가비지!
};

class PlayerThreatMonitor {
    PlayerController* pc;       // 가비지!
};
```

### GOOD

```cpp
// GOOD - 선언과 동시에 초기화
class Editor {
    GameObject* selectedObject = nullptr;  // 명시적 null 초기화
};

class PlayerController {
    IState* curState = nullptr;
    IState* fsmStates[9] = {};  // 모든 요소 nullptr로 초기화
};

class MiniGameManager {
    TextUI* mTimerUI     = nullptr;
    TextUI* mScoreUI     = nullptr;
    Image*  mGaugeBar    = nullptr;
    Image*  mGaugeFrame  = nullptr;
    Image*  mBackground  = nullptr;
    Image*  mResultUI    = nullptr;
};
```

---

## 유형 B: 미초기화 bool/숫자 멤버

### TigerEngine - W-55~W-57, W-59

```cpp
// BAD - bool 멤버 미초기화
class PlayerController {
    bool isPossiblePutFood;     // true일 수도 false일 수도 (가비지)
    bool isPossibleGetFood;     // 가비지
    bool isInputMoveForward;    // 가비지
    bool isInputMoveBackward;   // 가비지
    bool isInputMoveLeft;       // 가비지
    bool isInputMoveRight;      // 가비지
    bool isInputRun;            // 가비지
    bool isInputCrouch;         // 가비지
    bool isInputInteraction;    // 가비지
    // → 초기 프레임에서 예기치 않은 이동/상호작용 발생 가능
};

class SearchObject {
    bool hasItem;  // 가비지 → 아이템이 있는 것처럼 동작할 수 있음
};
```

### TigerEngine - B-7, B-8

```cpp
// BAD - 핸들/슬롯 멤버 미초기화
struct Slot {
    Object* ptr;         // 가비지
    uint32_t generation; // 가비지
    // generation이 가비지 값이면 핸들 유효성 검사 로직이 오동작
};

struct Handle {
    uint32_t index;      // 가비지
    uint32_t generation; // 가비지
};
```

### TigerEngine - C-22, R-12

```cpp
// BAD - 렌더링 관련 멤버 미초기화
class Light {
    LightType type;           // 가비지 → 잘못된 라이트 타입
    float intensity;          // 가비지 → 매우 밝거나 음수
    float range;              // 가비지
    float innerAngle;         // 가비지
    float outerAngle;         // 가비지
    Vector3 color;            // 가비지
};

struct RenderItem {
    ID3D11ShaderResourceView* textures[8];  // 가비지 포인터들!
    ID3D11Buffer* vertexBuffer;             // 가비지!
    uint32_t indexCount;                    // 가비지!
    uint32_t boneCount;                     // 가비지!
};
```

### GOOD

```cpp
// GOOD - C++11 멤버 초기화
class PlayerController {
    bool isPossiblePutFood = false;
    bool isPossibleGetFood = false;
    bool isInputMoveForward = false;
    // ...
};

struct Slot {
    Object* ptr = nullptr;
    uint32_t generation = 0;
};

struct Handle {
    uint32_t index = 0;
    uint32_t generation = 0;
};

class Light {
    LightType type = LightType::Point;
    float intensity = 1.0f;
    float range = 10.0f;
    float innerAngle = 30.0f;
    float outerAngle = 45.0f;
    Vector3 color = {1.0f, 1.0f, 1.0f};
};

struct RenderItem {
    ID3D11ShaderResourceView* textures[8] = {};
    ID3D11Buffer* vertexBuffer = nullptr;
    uint32_t indexCount = 0;
    uint32_t boneCount = 0;
};
```

---

## 유형 C: static 지역 변수의 씬 리로드 문제

### Scoopy - H09, H10, H11

```cpp
// BAD - static 변수가 씬 리로드 시 초기화되지 않음
void PlayerAnimController::Update() {
    static PSTAT prevStat;  // 첫 호출 때만 초기화, 씬 리로드 후에도 유지!
    if (stat != prevStat) {
        PlayAnimation(stat);
        prevStat = stat;
    }
}

void GameManager::Update() {
    static bool prevSetting = false;  // 씬 재시작 후에도 이전 값 유지!
    if (nowSetting != prevSetting) {
        // 씬 재시작 후 UI 상태 불일치
    }
}
```

### MiKuEngine - H-12

```cpp
// BAD - static stride가 재생성된 버퍼와 불일치 가능
void RenderSystem::DrawLocalLight() {
    static const UINT stride = m_sphereVB->GetBufferStride();
    // m_sphereVB가 재생성되면 stride가 stale 값
}
```

### GOOD

```cpp
// GOOD - 멤버 변수로 변경
class PlayerAnimController {
    PSTAT prevStat = PSTAT::Idle;  // 멤버 변수 → 씬 리로드 시 재생성

    void Update() {
        if (stat != prevStat) {
            PlayAnimation(stat);
            prevStat = stat;
        }
    }
};

// GOOD - static 제거
void RenderSystem::DrawLocalLight() {
    const UINT stride = m_sphereVB->GetBufferStride();  // 매 호출마다 갱신
    const UINT offset = 0;
}
```

---

## 유형 D: 미초기화 HRESULT 반환 처리 부재

### TigerEngine - M-7, M-18, M-19

```cpp
// BAD - CreateTexture2D 실패 시 null 텍스처로 계속 진행
ID3D11Texture2D* tex = nullptr;  // 이건 초기화됨
device->CreateTexture2D(&desc, nullptr, &tex);
// HRESULT 미검사! 실패하면 tex는 nullptr 유지

device->CreateRenderTargetView(tex, nullptr, &rtv);
// tex가 nullptr → CreateRenderTargetView 내부에서 크래시
```

### SPEngine (Scoopy 4Q) - H-02

```cpp
// BAD - D3D11CreateDevice 반환값 미검사
D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, NULL,
    0, nullptr, 0, D3D11_SDK_VERSION,
    device.GetAddressOf(), &featureLevel, nullptr);
// HRESULT 미검사! 실패하면 device는 nullptr
device.As(&m_pDevice);  // nullptr 접근 → 크래시
```

### GOOD

```cpp
// GOOD - HRESULT 검사
HRESULT hr = device->CreateTexture2D(&desc, nullptr, &tex);
if (FAILED(hr)) {
    LOG_ERROR("CreateTexture2D failed: 0x%08X", hr);
    return;
}

hr = device->CreateRenderTargetView(tex, nullptr, &rtv);
if (FAILED(hr)) {
    LOG_ERROR("CreateRenderTargetView failed: 0x%08X", hr);
    SAFE_RELEASE(tex);
    return;
}

// GOOD - HR_T 매크로로 일관 처리
HR_T(D3D11CreateDevice(...));  // 실패 시 예외 throw
```

---

## 발견 프로젝트별 분포

| 프로젝트 | 미초기화 이슈 수 | 대표 사례 |
|----------|-----------------|----------|
| TigerEngine | ~25건 | 포인터, bool, Handle, Light, RenderItem |
| Scoopy | ~4건 | static 변수 씬 리로드, HRESULT |
| SPEngine | ~2건 | D3D11CreateDevice, RenderPipe |
| EGOSIS | 0건 | (전반적으로 양호) |
| AliceEngine | ~3건 | 고정 버퍼, COM 참조 |
| MiKuEngine | ~3건 | static stride, bool, 해상도 |

---

## 핵심 교훈

1. **모든 멤버 변수를 선언 시 초기화하라** (`= nullptr`, `= 0`, `= false`, `= {}`)
2. **C++11 멤버 초기화 구문을 활용하라** (헤더에서 `int x = 0;`)
3. **static 지역 변수를 게임 로직에 사용하지 마라** (씬 리로드 시 재초기화 안 됨)
4. **HRESULT는 반드시 `SUCCEEDED()` 또는 `FAILED()`로 검사하라**
5. **구조체는 `= {}` 또는 `memset`으로 0 초기화하라**
6. **컴파일러 경고를 활성화하라** (`/W4`, `-Wuninitialized`)
