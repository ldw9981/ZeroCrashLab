/*============================================================================
 *  ZeroCrashLab - 11. Exception Types (예외의 종류와 동작)
 *  ---------------------------------------------------------------------------
 *  C++ 소프트웨어 예외와 SEH(Structured Exception Handling) 하드웨어 예외를
 *  직접 발생시켜 각각의 동작 차이를 관찰합니다.
 *
 *  [교육 목표]
 *  1. C++ 예외(throw)와 SEH 예외(하드웨어)의 차이를 이해한다.
 *  2. 각 예외 종류별로 프로그램이 어떻게 종료되는지 관찰한다.
 *  3. 예외 처리가 없으면 어떤 일이 일어나는지 체험한다.
 *
 *  [주의] 이 프로그램은 의도적으로 예외 처리를 하지 않습니다!
 *  각 항목 실행 시 프로그램이 크래시하므로, 항목별로 다시 실행하세요.
 *============================================================================*/
#include <iostream>
#include <string>
#include <vector>
#include <stdexcept>
#include <new>
#include <typeinfo>
#include <cstring>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

// ============================================================================
// C++ 표준 예외 (Standard Exceptions) - throw로 발생
// ============================================================================

// A: std::runtime_error - 런타임에 발생하는 일반 에러
void CppA_RuntimeError() {
    std::cout << "\n[C++ A] std::runtime_error\n";
    std::cout << "  게임에서 설정 파일 로드 실패, 리소스 초기화 실패 등에 사용.\n";
    std::cout << "  throw 후 catch가 없으면 std::terminate() → abort()\n\n";

    // 게임 시나리오: 셰이더 파일 로드 실패
    std::string shaderPath = "shaders/default.hlsl";
    std::cout << "  셰이더 로드 시도: " << shaderPath << "\n";
    throw std::runtime_error("Failed to load shader: " + shaderPath);
}

// B: std::out_of_range - 범위 밖 접근
void CppB_OutOfRange() {
    std::cout << "\n[C++ B] std::out_of_range\n";
    std::cout << "  vector::at(), string::at() 등에서 인덱스 초과 시 발생.\n";
    std::cout << "  operator[]와 달리 at()은 범위를 검사합니다.\n\n";

    std::vector<int> boneIndices = {0, 1, 2, 3, 4};
    std::cout << "  boneIndices 크기: " << boneIndices.size() << "\n";
    std::cout << "  boneIndices.at(999) 접근 시도...\n";
    int bone = boneIndices.at(999);  // throw std::out_of_range
    std::cout << "  bone = " << bone << "\n";  // 도달하지 않음
}

// C: std::bad_alloc - 메모리 할당 실패
void CppC_BadAlloc() {
    std::cout << "\n[C++ C] std::bad_alloc\n";
    std::cout << "  new로 메모리 할당이 실패하면 발생.\n";
    std::cout << "  매우 큰 메모리 요청으로 시뮬레이션합니다.\n\n";

    std::cout << "  100TB 메모리 할당 시도...\n";
    // 100TB 요청 - 어떤 시스템에서도 실패
    size_t hugeSize = static_cast<size_t>(100) * 1024 * 1024 * 1024 * 1024;
    char* p = new char[hugeSize];  // throw std::bad_alloc
    std::cout << "  할당 성공: " << (void*)p << "\n";  // 도달하지 않음
}

// D: std::invalid_argument - 잘못된 인자
void CppD_InvalidArgument() {
    std::cout << "\n[C++ D] std::invalid_argument\n";
    std::cout << "  std::stoi, std::stof 등에 변환 불가능한 문자열 전달 시 발생.\n\n";

    std::string configValue = "not_a_number";
    std::cout << "  설정값: \"" << configValue << "\"\n";
    std::cout << "  std::stoi() 변환 시도...\n";
    int value = std::stoi(configValue);  // throw std::invalid_argument
    std::cout << "  value = " << value << "\n";  // 도달하지 않음
}

// E: std::bad_cast - 잘못된 dynamic_cast (참조)
void CppE_BadCast() {
    std::cout << "\n[C++ E] std::bad_cast\n";
    std::cout << "  참조형 dynamic_cast가 실패하면 std::bad_cast를 throw.\n";
    std::cout << "  (포인터형 dynamic_cast는 nullptr을 반환하므로 throw 안 함)\n\n";

    struct Base { virtual ~Base() = default; };
    struct DerivedA : Base { int dataA = 1; };
    struct DerivedB : Base { int dataB = 2; };

    DerivedA objA;
    Base& baseRef = objA;

    std::cout << "  DerivedA 객체를 DerivedB& 로 dynamic_cast 시도...\n";
    DerivedB& refB = dynamic_cast<DerivedB&>(baseRef);  // throw std::bad_cast
    std::cout << "  refB.dataB = " << refB.dataB << "\n";  // 도달하지 않음
}

// F: 사용자 정의 예외 - 게임 엔진에서 흔한 패턴
class com_exception : public std::exception {
    HRESULT hr;
    char msg[128];
public:
    com_exception(HRESULT h) : hr(h) {
        sprintf_s(msg, "COM/DirectX Error: HRESULT 0x%08X", static_cast<unsigned int>(hr));
    }
    const char* what() const noexcept override { return msg; }
    HRESULT get_hr() const { return hr; }
};

// HR_T 매크로 시뮬레이션 (실제 게임 엔진에서 자주 사용)
inline void HR_T(HRESULT hr) {
    if (FAILED(hr)) throw com_exception(hr);
}

void CppF_ComException() {
    std::cout << "\n[C++ F] 사용자 정의 예외 (com_exception)\n";
    std::cout << "  DirectX/COM 호출 실패 시 HR_T 매크로가 throw.\n";
    std::cout << "  SPEngine 보고서: 프로젝트 전체에 catch 없이 HR_T 사용!\n\n";

    std::cout << "  HR_T(E_OUTOFMEMORY) 호출...\n";
    HR_T(E_OUTOFMEMORY);  // throw com_exception
    std::cout << "  성공!\n";  // 도달하지 않음
}

// G: 정수형 throw - 안티패턴이지만 실존
void CppG_ThrowInt() {
    std::cout << "\n[C++ G] throw int (비표준 예외)\n";
    std::cout << "  C++에서는 아무 타입이나 throw 가능.\n";
    std::cout << "  catch(std::exception&)으로 잡히지 않음!\n";
    std::cout << "  catch(...)만 잡을 수 있음.\n\n";

    std::cout << "  throw 42 실행...\n";
    throw 42;  // catch(std::exception&)으로 잡히지 않음!
}

// H: 문자열 throw - 또 다른 안티패턴
void CppH_ThrowString() {
    std::cout << "\n[C++ H] throw const char* (비표준 예외)\n";
    std::cout << "  레거시 C++ 코드에서 가끔 발견됨.\n";
    std::cout << "  catch(std::exception&)으로 잡히지 않음!\n\n";

    std::cout << "  throw \"Shader compilation failed\" 실행...\n";
    throw "Shader compilation failed";
}

// ============================================================================
// SEH 예외 (Structured Exception Handling) - 하드웨어/OS가 발생
// ============================================================================

// I: Access Violation (0xC0000005) - nullptr 역참조
void SehI_AccessViolation_Null() {
    std::cout << "\n[SEH I] Access Violation - nullptr 역참조\n";
    std::cout << "  예외 코드: EXCEPTION_ACCESS_VIOLATION (0xC0000005)\n";
    std::cout << "  C++ try-catch로 잡히지 않음! SEH 또는 VEH만 처리 가능.\n\n";

    int* p = nullptr;
    std::cout << "  nullptr에 쓰기 시도...\n";
    *p = 42;  // EXCEPTION_ACCESS_VIOLATION
    std::cout << "  성공!\n";  // 도달하지 않음
}

// J: Access Violation - 해제된 메모리 접근
void SehJ_AccessViolation_Freed() {
    std::cout << "\n[SEH J] Access Violation - 해제된 메모리 접근\n";
    std::cout << "  예외 코드: EXCEPTION_ACCESS_VIOLATION (0xC0000005)\n";
    std::cout << "  Debug 빌드에서 delete 후 메모리가 0xDD로 채워집니다.\n\n";

    int* data = new int[100];
    data[0] = 12345;
    std::cout << "  할당 후 data[0] = " << data[0] << "\n";

    delete[] data;
    std::cout << "  delete 후 data에 접근 시도...\n";

    // Debug 빌드에서는 페이지가 해제되어 AV 발생 가능
    // 더 확실하게 만들기 위해 VirtualFree 사용
    void* mem = VirtualAlloc(nullptr, 4096, MEM_COMMIT, PAGE_READWRITE);
    int* ptr = static_cast<int*>(mem);
    *ptr = 99999;
    std::cout << "  VirtualAlloc 후 *ptr = " << *ptr << "\n";
    VirtualFree(mem, 0, MEM_RELEASE);
    std::cout << "  VirtualFree 후 *ptr 접근 시도...\n";
    int val = *ptr;  // EXCEPTION_ACCESS_VIOLATION
    std::cout << "  val = " << val << "\n";  // 도달하지 않음
}

// K: Access Violation - 실행 불가 메모리 실행
void SehK_AccessViolation_DEP() {
    std::cout << "\n[SEH K] Access Violation - DEP (Data Execution Prevention)\n";
    std::cout << "  예외 코드: EXCEPTION_ACCESS_VIOLATION (0xC0000005)\n";
    std::cout << "  데이터 영역의 코드를 실행하려 하면 발생.\n\n";

    // 실행 권한 없는 메모리에 코드를 넣고 실행 시도
    unsigned char code[] = { 0xC3 };  // ret 명령어
    void* mem = VirtualAlloc(nullptr, 4096, MEM_COMMIT, PAGE_READWRITE);  // 실행 불가!
    memcpy(mem, code, sizeof(code));

    std::cout << "  PAGE_READWRITE 메모리에서 코드 실행 시도...\n";
    using FuncPtr = void(*)();
    FuncPtr func = reinterpret_cast<FuncPtr>(mem);
    func();  // EXCEPTION_ACCESS_VIOLATION (DEP)
    VirtualFree(mem, 0, MEM_RELEASE);
}

// L: Integer Divide by Zero (0xC0000094)
void SehL_IntDivideByZero() {
    std::cout << "\n[SEH L] Integer Divide by Zero\n";
    std::cout << "  예외 코드: EXCEPTION_INT_DIVIDE_BY_ZERO (0xC0000094)\n";
    std::cout << "  정수 나눗셈에서만 발생. 실수(float/double)는 NaN/Inf가 됨.\n\n";

    volatile int a = 100;
    volatile int b = 0;
    std::cout << "  " << a << " / " << b << " 계산 시도...\n";
    int result = a / b;  // EXCEPTION_INT_DIVIDE_BY_ZERO
    std::cout << "  result = " << result << "\n";  // 도달하지 않음
}

// M: Stack Overflow (0xC00000FD)
void SehM_StackOverflow_Helper(int depth) {
    volatile char buffer[4096];  // 프레임마다 4KB 스택 사용
    buffer[0] = static_cast<char>(depth);
    if (depth % 1000 == 0)
        std::cout << "    재귀 깊이: " << depth << "\n";
    SehM_StackOverflow_Helper(depth + 1);
}

void SehM_StackOverflow() {
    std::cout << "\n[SEH M] Stack Overflow\n";
    std::cout << "  예외 코드: EXCEPTION_STACK_OVERFLOW (0xC00000FD)\n";
    std::cout << "  무한 재귀 또는 매우 깊은 재귀에서 발생.\n";
    std::cout << "  기본 스택 크기 1MB를 초과하면 크래시.\n\n";

    std::cout << "  무한 재귀 시작...\n";
    SehM_StackOverflow_Helper(0);
}

// N: Privileged Instruction (0xC0000096)
void SehN_PrivilegedInstruction() {
    std::cout << "\n[SEH N] Privileged Instruction\n";
    std::cout << "  예외 코드: EXCEPTION_PRIV_INSTRUCTION (0xC0000096)\n";
    std::cout << "  커널 모드 전용 명령어를 유저 모드에서 실행 시 발생.\n\n";

    std::cout << "  HLT (CPU 정지) 명령어 실행 시도...\n";
#ifdef _M_X64
    // hlt 명령어 - Ring 0(커널)에서만 실행 가능
    unsigned char hlt_code[] = { 0xF4 };  // HLT
    void* mem = VirtualAlloc(nullptr, 4096, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    memcpy(mem, hlt_code, sizeof(hlt_code));
    using FuncPtr = void(*)();
    FuncPtr func = reinterpret_cast<FuncPtr>(mem);
    func();  // EXCEPTION_PRIV_INSTRUCTION
    VirtualFree(mem, 0, MEM_RELEASE);
#else
    std::cout << "  (x64 빌드에서만 지원)\n";
#endif
}

// O: Illegal Instruction (0xC000001D)
void SehO_IllegalInstruction() {
    std::cout << "\n[SEH O] Illegal Instruction\n";
    std::cout << "  예외 코드: EXCEPTION_ILLEGAL_INSTRUCTION (0xC000001D)\n";
    std::cout << "  CPU가 해석할 수 없는 잘못된 opcode 실행 시 발생.\n\n";

    // UD2 - 의도적으로 invalid opcode를 발생시키는 명령어
    unsigned char ud2_code[] = { 0x0F, 0x0B };  // UD2
    void* mem = VirtualAlloc(nullptr, 4096, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    memcpy(mem, ud2_code, sizeof(ud2_code));

    std::cout << "  UD2 (Undefined Instruction) 실행 시도...\n";
    using FuncPtr = void(*)();
    FuncPtr func = reinterpret_cast<FuncPtr>(mem);
    func();  // EXCEPTION_ILLEGAL_INSTRUCTION
    VirtualFree(mem, 0, MEM_RELEASE);
}

// P: Breakpoint (0x80000003) - 디버거 인터럽트
void SehP_Breakpoint() {
    std::cout << "\n[SEH P] Breakpoint Exception\n";
    std::cout << "  예외 코드: EXCEPTION_BREAKPOINT (0x80000003)\n";
    std::cout << "  __debugbreak() 또는 INT 3 명령어로 발생.\n";
    std::cout << "  디버거가 연결되어 있으면 디버거가 잡고,\n";
    std::cout << "  디버거 없이 실행하면 크래시합니다.\n\n";

    std::cout << "  __debugbreak() 호출...\n";
    __debugbreak();  // EXCEPTION_BREAKPOINT (INT 3)
    std::cout << "  디버거에서 계속 실행하면 여기 도달.\n";
}

// ============================================================================
// 특수 종료 케이스
// ============================================================================

// Q: std::abort() - 즉시 종료 (시그널 기반)
void SpecialQ_Abort() {
    std::cout << "\n[특수 Q] std::abort()\n";
    std::cout << "  SIGABRT 시그널을 발생시켜 즉시 종료.\n";
    std::cout << "  assert() 실패, std::terminate() 내부에서 호출됨.\n";
    std::cout << "  try-catch로 잡을 수 없음. 소멸자도 호출되지 않음.\n\n";

    std::cout << "  std::abort() 호출...\n";
    std::abort();
}

// R: std::terminate() - 미처리 예외의 최종 도착지
void SpecialR_Terminate() {
    std::cout << "\n[특수 R] std::terminate()\n";
    std::cout << "  C++ 예외가 catch 없이 main을 빠져나가면 호출됨.\n";
    std::cout << "  noexcept 함수에서 예외가 throw되면 호출됨.\n";
    std::cout << "  기본 동작: std::abort() 호출.\n\n";

    std::cout << "  std::terminate() 직접 호출...\n";
    std::terminate();
}

// S: noexcept 위반
void SpecialS_NoexceptViolation_Inner() noexcept {
    throw std::runtime_error("noexcept 함수에서 throw!");
}

void SpecialS_NoexceptViolation() {
    std::cout << "\n[특수 S] noexcept 위반\n";
    std::cout << "  noexcept로 선언된 함수에서 예외가 발생하면\n";
    std::cout << "  std::terminate()가 즉시 호출됩니다.\n";
    std::cout << "  스택 해제(unwind)도 보장되지 않습니다.\n\n";

    std::cout << "  noexcept 함수에서 throw 시도...\n";
    SpecialS_NoexceptViolation_Inner();
}

// ============================================================================
// 메인
// ============================================================================
int main() {
    std::cout << "====================================================\n";
    std::cout << "  ZeroCrashLab - 11. Exception Types\n";
    std::cout << "  (예외의 종류와 동작)\n";
    std::cout << "====================================================\n";
    std::cout << "\n  [주의] 각 항목은 프로그램을 크래시시킵니다!\n";
    std::cout << "  항목 실행 후 프로그램을 다시 시작하세요.\n";
    std::cout << "\n";
    std::cout << "  ──── C++ 표준 예외 (throw) ────\n";
    std::cout << "  [A] std::runtime_error   (런타임 에러)\n";
    std::cout << "  [B] std::out_of_range    (범위 초과)\n";
    std::cout << "  [C] std::bad_alloc       (메모리 할당 실패)\n";
    std::cout << "  [D] std::invalid_argument(잘못된 인자)\n";
    std::cout << "  [E] std::bad_cast        (잘못된 dynamic_cast)\n";
    std::cout << "  [F] com_exception        (DirectX/COM 에러)\n";
    std::cout << "  [G] throw int            (비표준 - 정수 throw)\n";
    std::cout << "  [H] throw const char*    (비표준 - 문자열 throw)\n";
    std::cout << "\n";
    std::cout << "  ──── SEH 예외 (하드웨어/OS) ────\n";
    std::cout << "  [I] Access Violation     (nullptr 역참조)\n";
    std::cout << "  [J] Access Violation     (해제된 메모리 접근)\n";
    std::cout << "  [K] Access Violation     (DEP - 데이터 실행)\n";
    std::cout << "  [L] Int Divide by Zero   (정수 0 나누기)\n";
    std::cout << "  [M] Stack Overflow       (스택 오버플로우)\n";
    std::cout << "  [N] Privileged Instruction (권한 없는 명령어)\n";
    std::cout << "  [O] Illegal Instruction  (잘못된 CPU 명령어)\n";
    std::cout << "  [P] Breakpoint           (디버거 인터럽트)\n";
    std::cout << "\n";
    std::cout << "  ──── 특수 종료 ────\n";
    std::cout << "  [Q] std::abort()         (SIGABRT 즉시 종료)\n";
    std::cout << "  [R] std::terminate()     (미처리 예외 종료)\n";
    std::cout << "  [S] noexcept 위반        (terminate 호출)\n";
    std::cout << "\n";
    std::cout << "  [X] 종료\n";
    std::cout << "----------------------------------------------------\n";

    while (true) {
        std::cout << "\n선택> ";
        char choice;
        std::cin >> choice;
        choice = toupper(choice);

        switch (choice) {
        // C++ 표준 예외
        case 'A': CppA_RuntimeError(); break;
        case 'B': CppB_OutOfRange(); break;
        case 'C': CppC_BadAlloc(); break;
        case 'D': CppD_InvalidArgument(); break;
        case 'E': CppE_BadCast(); break;
        case 'F': CppF_ComException(); break;
        case 'G': CppG_ThrowInt(); break;
        case 'H': CppH_ThrowString(); break;

        // SEH 예외
        case 'I': SehI_AccessViolation_Null(); break;
        case 'J': SehJ_AccessViolation_Freed(); break;
        case 'K': SehK_AccessViolation_DEP(); break;
        case 'L': SehL_IntDivideByZero(); break;
        case 'M': SehM_StackOverflow(); break;
        case 'N': SehN_PrivilegedInstruction(); break;
        case 'O': SehO_IllegalInstruction(); break;
        case 'P': SehP_Breakpoint(); break;

        // 특수 종료
        case 'Q': SpecialQ_Abort(); break;
        case 'R': SpecialR_Terminate(); break;
        case 'S': SpecialS_NoexceptViolation(); break;

        case 'X': std::cout << "종료합니다.\n"; return 0;
        default:  std::cout << "잘못된 입력입니다.\n"; break;
        }
    }
}
