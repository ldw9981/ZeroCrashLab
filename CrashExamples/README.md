# ZeroCrashLab - Crash Risk Pattern Examples

> 6개 게임 엔진 프로젝트의 **389건** 크래시 위험 이슈에서 추출한 유형별 실제 사례집

---

## 분석 대상 프로젝트

| # | 프로젝트 | 엔진 | 분석 파일 수 | 발견 이슈 |
|---|---------|------|-------------|----------|
| 1 | **TigerEngine** (JesaSang) | DirectX11 게임 엔진 | ~160 | **223건** |
| 2 | **Scoopy** (Tower Defense) | MMMEngine | ~90 | **35건** |
| 3 | **SPEngine** (Scoopy 4Q) | DirectX11 그래픽 엔진 | ~68 | **26건** |
| 4 | **EGOSIS** | AliceRenderer | 대규모 | **32건** |
| 5 | **AliceEngine** | DirectX11 3D 엔진 | ~384 | **25건** |
| 6 | **MiKuEngine** (FragileFall) | Custom Engine | ~503 | **48건** |

---

## 유형별 가이드

### 즉시 크래시 유형

| # | 유형 | 파일 | 빈도 | 위험도 |
|---|------|------|------|--------|
| 01 | [**Null Pointer Dereference**](01_NullPointerDereference.md) | 널 포인터 역참조 | ★★★★★ | CRITICAL |
| 02 | [**Use-After-Free / Dangling Pointer**](02_UseAfterFree_DanglingPointer.md) | 해제 후 사용 | ★★★★☆ | CRITICAL |
| 03 | [**Iterator Invalidation**](03_IteratorInvalidation.md) | 반복자 무효화 | ★★★☆☆ | CRITICAL |

### 메모리 문제 유형

| # | 유형 | 파일 | 빈도 | 위험도 |
|---|------|------|------|--------|
| 04 | [**Memory Leak**](04_MemoryLeak.md) | 메모리 누수 | ★★★★☆ | HIGH |
| 07 | [**Buffer Overflow / Out-of-Bounds**](07_BufferOverflow_OutOfBounds.md) | 범위 밖 접근 | ★★★☆☆ | CRITICAL |

### 연산 오류 유형

| # | 유형 | 파일 | 빈도 | 위험도 |
|---|------|------|------|--------|
| 05 | [**Division by Zero / NaN**](05_DivisionByZero.md) | 0 나누기 | ★★★☆☆ | HIGH |

### 초기화 / 상태 문제 유형

| # | 유형 | 파일 | 빈도 | 위험도 |
|---|------|------|------|--------|
| 06 | [**Uninitialized Variable**](06_UninitializedVariable.md) | 미초기화 변수 | ★★★★☆ | HIGH |
| 08 | [**Race Condition / Thread Safety**](08_RaceCondition_ThreadSafety.md) | 경쟁 조건 | ★★☆☆☆ | HIGH |

### 코드 품질 유형

| # | 유형 | 파일 | 빈도 | 위험도 |
|---|------|------|------|--------|
| 09 | [**Missing Return / Undefined Behavior**](09_MissingReturnValue_UndefinedBehavior.md) | 반환값 누락 / UB | ★★★☆☆ | CRITICAL |
| 10 | [**Logic Error**](10_LogicError.md) | 논리 오류 | ★★★☆☆ | MEDIUM~HIGH |

---

## 유형별 이슈 분포 (전체 389건)

```
Null Pointer Dereference  ████████████████████████████████████  ~147건 (38%)
Uninitialized Variable    ████████████                          ~37건 (10%)
Memory Leak               ██████████                            ~22건 (6%)
Use-After-Free            █████████                             ~31건 (8%)
Iterator Invalidation     ██████                                ~12건 (3%)
Buffer Overflow / OOB     ████████                              ~19건 (5%)
Division by Zero          █████                                 ~11건 (3%)
Race Condition            █████                                 ~16건 (4%)
Missing Return / UB       ██████                                ~18건 (5%)
Logic Error               ██████████████                        ~26건 (7%)
기타 (HRESULT 등)          █████████████                         ~50건 (13%)
```

---

## 프로젝트별 이슈 분포

```
TigerEngine    ████████████████████████████████████████████████  223건
MiKuEngine     ████████████                                      48건
Scoopy         ████████                                          35건
EGOSIS         ███████                                           32건
SPEngine       ██████                                            26건
AliceEngine    █████                                             25건
```

---

## 6개 프로젝트 공통 패턴 Top 5

### 1. GetComponent/Find 체인의 null 미검사 (6/6 프로젝트)
모든 프로젝트에서 `GetComponent<T>()`, `Find()`, `GetGameObjectByName()` 등의 반환값을 검사 없이 체인 호출하는 패턴이 발견됨.

### 2. 멤버 변수 미초기화 (5/6 프로젝트)
포인터, bool, 숫자 멤버를 선언만 하고 초기화하지 않아 가비지 값으로 동작.

### 3. SAFE_RELEASE/SAFE_DELETE 값 전달 버그 (2/6 프로젝트, 동일 코드)
TigerEngine과 SPEngine에서 동일한 코드 패턴 발견 - 포인터를 값으로 전달하여 null 설정이 실패.

### 4. 반복자 무효화 (5/6 프로젝트)
range-for 루프 내에서 컬렉션을 수정하거나, 콜백에서 간접적으로 수정하는 패턴.

### 5. 싱글턴 인스턴스 null 미검사 (4/6 프로젝트)
매니저 싱글턴의 초기화 순서에 의존하면서 null 검사를 하지 않는 패턴.

---

## 권장 방어 전략

### 코딩 컨벤션
- 모든 멤버 변수를 선언 시 초기화 (`= nullptr`, `= 0`, `= false`)
- 모든 `GetComponent`, `Find`, `Instantiate` 반환값 null 검사
- 중괄호 항상 사용 (dangling else 방지)
- `auto` 사용 시 `auto&` vs `auto` 명시적 구분

### 도구 활용
- 컴파일러 경고 최대화 (`/W4`, `-Wall -Wextra`)
- 정적 분석 도구 도입 (PVS-Studio, clang-tidy)
- Debug 빌드에서 AddressSanitizer 활성화
- `std::unique_ptr`, `ComPtr<T>` 적극 활용

### 설계 패턴
- Deferred Queue: 콜백/루프 내 변경사항은 큐에 넣고 나중에 처리
- SafeGet 헬퍼: null-safe 접근 래퍼 함수 도입
- Handle 기반 약한 참조: raw pointer 대신 generation-based handle
- 2-pass 삭제: 수집 → 콜백 → 실제 삭제 분리

---

*Generated: 2026-02-11 | 6개 프로젝트 389건 이슈 분석 기반*
