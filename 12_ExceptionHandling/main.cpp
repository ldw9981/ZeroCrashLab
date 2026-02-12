/*============================================================================
 *  ZeroCrashLab - 12. Exception Handling (예외 처리 방법)
 *  ---------------------------------------------------------------------------
 *  11번에서 예외를 "발생"시켜 봤다면, 이번에는 예외를 "처리"하는 방법을
 *  단계별로 체험합니다. C++ try-catch, SEH __try/__except, 미니덤프 생성까지
 *  실전에서 사용하는 예외 처리 패턴을 학습합니다.
 *
 *  [교육 목표]
 *  1. C++ try-catch로 표준 예외를 잡고 처리하는 방법을 익힌다.
 *  2. SEH __try/__except로 하드웨어 예외를 잡는 방법을 익힌다.
 *  3. MiniDumpWriteDump로 크래시 덤프를 남기는 방법을 익힌다.
 *  4. 게임 엔진에서 사용하는 실전 예외 처리 패턴을 이해한다.
 *
 *  [컴파일러 설정]
 *  이 프로젝트는 /EHa (비동기 예외 처리) 옵션이 필요합니다.
 *  /EHa를 사용하면 SEH와 C++ 예외를 동시에 처리할 수 있습니다.
 *============================================================================*/
#include <iostream>
#include <string>
#include <vector>
#include <stdexcept>
#include <new>
#include <memory>
#include <functional>
#include <cstring>
#include <cstdio>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <DbgHelp.h>

#pragma comment(lib, "DbgHelp.lib")

// BuildInfo.h - Pre-Build Event에서 자동 생성됨
// Git revision, branch, 빌드 타임스탬프 정보를 담고 있습니다.
#include "BuildInfo.h"

// ============================================================================
// 빌드 정보 전역 변수 (덤프에서 확인 가능)
// ============================================================================
// 방법 1: .pdb가 있으면 VS 조사식(Watch)에서 변수명으로 확인
// 방법 2: .pdb가 없어도 바이너리 에디터(HxD 등)에서 "<<<BUILDTAG>>>" 텍스트 검색
//
// #pragma optimize("", off)로 최적화 방지 → 덤프에서 반드시 살아있음
#pragma optimize("", off)
volatile const char* g_BuildGitRevision = BUILD_GIT_REVISION;
volatile const char* g_BuildGitBranch   = BUILD_GIT_BRANCH;
volatile const char* g_BuildTimestamp   = BUILD_TIMESTAMP;

// .pdb 없이도 덤프에서 빌드 정보를 찾을 수 있는 검색용 태그 문자열
// HxD 등 바이너리 에디터에서 "<<<BUILDTAG>>>" 로 검색하면 바로 찾을 수 있음
extern "C" const char g_BuildTag[] =
    "<<<BUILDTAG>>> "
    "Rev:" BUILD_GIT_REVISION " "
    "Branch:" BUILD_GIT_BRANCH " "
    "Date:" BUILD_GIT_DATE " "
    "Built:" BUILD_TIMESTAMP " "
    "<<<END_BUILDTAG>>>";

// 링커가 g_BuildTag를 제거하지 못하게 강제 포함
#pragma comment(linker, "/include:g_BuildTag")
#pragma optimize("", on)

// ============================================================================
// 유틸리티: 미니덤프 생성 함수
// ============================================================================

// SEH 필터에서 사용할 미니덤프 생성 함수
bool WriteMiniDump(EXCEPTION_POINTERS* pExInfo, const wchar_t* filename)
{
    HANDLE hFile = CreateFileW(
        filename,
        GENERIC_WRITE,
        0,
        NULL,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (hFile == INVALID_HANDLE_VALUE)
    {
        std::cout << "  [ERROR] 덤프 파일 생성 실패!\n";
        return false;
    }

    MINIDUMP_EXCEPTION_INFORMATION mei;
    mei.ThreadId          = GetCurrentThreadId();
    mei.ExceptionPointers = pExInfo;
    mei.ClientPointers    = FALSE;

    BOOL result = MiniDumpWriteDump(
        GetCurrentProcess(),
        GetCurrentProcessId(),
        hFile,
        MiniDumpNormal,  // 기본 덤프 (콜스택 + 레지스터 + 스레드 정보)
        &mei,
        NULL,
        NULL
    );

    CloseHandle(hFile);
    return result != FALSE;
}

// 브랜치명에 '/' 등 파일명에 쓸 수 없는 문자를 '_'로 치환
// 예: "claude/infallible-dubinsky" → "claude_infallible-dubinsky"
void MakeSafeFilename(char* dst, size_t dstSize, const char* src)
{
    size_t i = 0;
    for (; i < dstSize - 1 && src[i] != '\0'; ++i)
    {
        char c = src[i];
        if (c == '/' || c == '\\' || c == ':' || c == '*' ||
            c == '?' || c == '"'  || c == '<' || c == '>' || c == '|')
            dst[i] = '_';
        else
            dst[i] = c;
    }
    dst[i] = '\0';
}

void MakeSafeFilenameW(wchar_t* dst, size_t dstSize, const wchar_t* src)
{
    size_t i = 0;
    for (; i < dstSize - 1 && src[i] != L'\0'; ++i)
    {
        wchar_t c = src[i];
        if (c == L'/' || c == L'\\' || c == L':' || c == L'*' ||
            c == L'?' || c == L'"'  || c == L'<' || c == L'>' || c == L'|')
            dst[i] = L'_';
        else
            dst[i] = c;
    }
    dst[i] = L'\0';
}

// SEH 예외 코드를 문자열로 변환
const char* GetExceptionCodeString(DWORD code)
{
    switch (code)
    {
    case EXCEPTION_ACCESS_VIOLATION:      return "EXCEPTION_ACCESS_VIOLATION (0xC0000005)";
    case EXCEPTION_INT_DIVIDE_BY_ZERO:    return "EXCEPTION_INT_DIVIDE_BY_ZERO (0xC0000094)";
    case EXCEPTION_STACK_OVERFLOW:         return "EXCEPTION_STACK_OVERFLOW (0xC00000FD)";
    case EXCEPTION_ILLEGAL_INSTRUCTION:   return "EXCEPTION_ILLEGAL_INSTRUCTION (0xC000001D)";
    case EXCEPTION_PRIV_INSTRUCTION:      return "EXCEPTION_PRIV_INSTRUCTION (0xC0000096)";
    case EXCEPTION_BREAKPOINT:            return "EXCEPTION_BREAKPOINT (0x80000003)";
    case EXCEPTION_FLT_DIVIDE_BY_ZERO:    return "EXCEPTION_FLT_DIVIDE_BY_ZERO (0xC000008E)";
    default:                              return "UNKNOWN";
    }
}

// EXCEPTION_POINTERS에서 크래시 원인 메시지를 생성
// 콘솔에 출력하고, MessageBox용 문자열도 만든다.
// bAskDump = true이면 "덤프를 남기겠습니까?" MessageBox를 띄우고
// 사용자가 '예'를 누르면 true 리턴, '아니오'면 false 리턴
// bAskDump = false이면 MessageBox 없이 콘솔 출력만 (true 리턴)
bool PrintCrashMessage(EXCEPTION_POINTERS* pExInfo, bool bAskDump = false)
{
    EXCEPTION_RECORD* pRecord = pExInfo->ExceptionRecord;
    CONTEXT* pCtx = pExInfo->ContextRecord;
    DWORD code = pRecord->ExceptionCode;

    // MessageBox용 문자열 조립
    char msgBuf[1024];
    int pos = 0;

    pos += sprintf_s(msgBuf + pos, sizeof(msgBuf) - pos,
        "[ CRASH ]\n\nCode: %s\n", GetExceptionCodeString(code));

    pos += sprintf_s(msgBuf + pos, sizeof(msgBuf) - pos,
        "Address: 0x%llX\n",
        reinterpret_cast<unsigned long long>(pRecord->ExceptionAddress));

    // 콘솔 출력
    std::cout << "  ┌─── Crash Info ───────────────────────────────┐\n";
    std::cout << "  │ Code:    " << GetExceptionCodeString(code) << "\n";
    std::cout << "  │ Address: 0x" << std::hex
              << reinterpret_cast<uintptr_t>(pRecord->ExceptionAddress)
              << std::dec << "\n";

    // 예외별 진단 메시지
    const char* cause = "";
    const char* diagnosis = "";

    if (code == EXCEPTION_ACCESS_VIOLATION && pRecord->NumberParameters >= 2)
    {
        ULONG_PTR rwFlag  = pRecord->ExceptionInformation[0];
        ULONG_PTR target  = pRecord->ExceptionInformation[1];

        const char* action = "Read";
        if (rwFlag == 1) action = "Write";
        else if (rwFlag == 8) action = "DEP Execute";

        pos += sprintf_s(msgBuf + pos, sizeof(msgBuf) - pos,
            "\n%s at 0x%llX\n", action, static_cast<unsigned long long>(target));

        std::cout << "  │ 원인:    0x" << std::hex << target << std::dec
                  << " 주소에 " << (rwFlag == 0 ? "읽기" : rwFlag == 1 ? "쓰기" : "DEP 실행") << " 시도\n";

        if (target == 0)
        {
            diagnosis = "nullptr dereference!\nCheck pointer initialization.";
            std::cout << "  │ 진단:    nullptr 역참조! 포인터 초기화를 확인하세요.\n";
        }
        else if (target < 0x10000)
        {
            diagnosis = "Low address access.\nBase pointer may be nullptr\n(accessing struct member with null base).";
            std::cout << "  │ 진단:    낮은 주소 접근 → 구조체 멤버 오프셋일 수 있음\n"
                      << "  │          (base 포인터가 nullptr인 상태에서 멤버 접근)\n";
        }
        else
        {
            diagnosis = "Accessing freed or invalid memory.";
            std::cout << "  │ 진단:    해제된 메모리 또는 잘못된 포인터 접근\n";
        }
    }
    else if (code == EXCEPTION_INT_DIVIDE_BY_ZERO)
    {
        diagnosis = "Integer division by zero.\nAdd zero-check before division.";
        std::cout << "  │ 원인:    정수 나눗셈에서 제수(divisor)가 0\n";
        std::cout << "  │ 진단:    나누기 전에 0 체크를 추가하세요.\n";
    }
    else if (code == EXCEPTION_STACK_OVERFLOW)
    {
        diagnosis = "Stack memory exceeded (default 1MB).\nInfinite recursion or large stack allocation.";
        std::cout << "  │ 원인:    스택 메모리 초과 (기본 1MB)\n";
        std::cout << "  │ 진단:    무한 재귀 또는 스택에 너무 큰 배열 할당\n";
    }
    else if (code == EXCEPTION_ILLEGAL_INSTRUCTION)
    {
        diagnosis = "Invalid CPU instruction.\nFunction pointer error or memory corruption.";
        std::cout << "  │ 원인:    CPU가 해석할 수 없는 명령어 실행\n";
        std::cout << "  │ 진단:    함수 포인터 오류 또는 메모리 오염 가능성\n";
    }
    else if (code == EXCEPTION_BREAKPOINT)
    {
        diagnosis = "__debugbreak() or Assert failure.\nCheck the assert condition.";
        std::cout << "  │ 원인:    __debugbreak() 또는 Assert 실패\n";
        std::cout << "  │ 진단:    Assert 조건을 확인하세요.\n";
    }

    if (diagnosis[0] != '\0')
    {
        pos += sprintf_s(msgBuf + pos, sizeof(msgBuf) - pos,
            "\nDiagnosis:\n%s\n", diagnosis);
    }

    // 레지스터 정보 (x64)
#ifdef _M_X64
    std::cout << "  │ RIP:     0x" << std::hex << pCtx->Rip << std::dec << "\n";
    std::cout << "  │ RSP:     0x" << std::hex << pCtx->Rsp << std::dec << "\n";
    std::cout << "  │ RBP:     0x" << std::hex << pCtx->Rbp << std::dec << "\n";

    pos += sprintf_s(msgBuf + pos, sizeof(msgBuf) - pos,
        "\nRIP: 0x%llX\nRSP: 0x%llX\nRBP: 0x%llX\n",
        pCtx->Rip, pCtx->Rsp, pCtx->Rbp);
#endif

    pos += sprintf_s(msgBuf + pos, sizeof(msgBuf) - pos,
        "\nBuild: %s (%s)", BUILD_GIT_REVISION, BUILD_GIT_BRANCH);

    std::cout << "  │ Build:   " << BUILD_GIT_REVISION << " (" << BUILD_GIT_BRANCH << ")\n";
    std::cout << "  └──────────────────────────────────────────────┘\n";

    // MessageBox로 크래시 정보 표시 + 덤프 저장 여부 확인
    if (bAskDump)
    {
        // 덤프 질문을 메시지 끝에 추가
        char fullMsg[1280];
        sprintf_s(fullMsg, sizeof(fullMsg),
            "%s\n\n──────────────────────\nCrash dump를 저장하시겠습니까?", msgBuf);

        int result = MessageBoxA(
            NULL,
            fullMsg,
            "ZeroCrashLab - Crash Detected",
            MB_YESNO | MB_ICONERROR | MB_TOPMOST
        );

        return (result == IDYES);
    }

    return true;  // bAskDump=false면 항상 true
}

// ============================================================================
// A: C++ try-catch 기본 - std::exception 계열 잡기
// ============================================================================

void HandleA_TryCatchBasic()
{
    std::cout << "\n[A] C++ try-catch 기본 (std::exception 계열 잡기)\n";
    std::cout << "============================================================\n";
    std::cout << "  catch 순서가 중요합니다: 파생 클래스 → 기반 클래스 순으로!\n\n";

    // 1) std::out_of_range (std::logic_error의 파생)
    std::cout << "  --- 1) std::out_of_range ---\n";
    try
    {
        std::vector<int> v = {1, 2, 3};
        std::cout << "  v.at(999) 접근 시도...\n";
        int val = v.at(999);  // throw std::out_of_range
        std::cout << "  val = " << val << "\n";
    }
    catch (const std::out_of_range& e)
    {
        std::cout << "  [CAUGHT] std::out_of_range: " << e.what() << "\n";
        std::cout << "  → 프로그램이 죽지 않고 계속 실행됩니다!\n";
    }

    // 2) std::runtime_error
    std::cout << "\n  --- 2) std::runtime_error ---\n";
    try
    {
        std::cout << "  셰이더 로드 실패 시뮬레이션...\n";
        throw std::runtime_error("Failed to load shader: default.hlsl");
    }
    catch (const std::runtime_error& e)
    {
        std::cout << "  [CAUGHT] std::runtime_error: " << e.what() << "\n";
    }

    // 3) std::bad_alloc
    std::cout << "\n  --- 3) std::bad_alloc ---\n";
    try
    {
        std::cout << "  100TB 메모리 할당 시도...\n";
        size_t hugeSize = static_cast<size_t>(100) * 1024 * 1024 * 1024 * 1024;
        char* p = new char[hugeSize];  // throw std::bad_alloc
        delete[] p;
    }
    catch (const std::bad_alloc& e)
    {
        std::cout << "  [CAUGHT] std::bad_alloc: " << e.what() << "\n";
        std::cout << "  → 메모리 부족 상황을 안전하게 처리했습니다.\n";
    }

    // 4) catch 순서의 중요성
    std::cout << "\n  --- 4) catch 순서 데모 ---\n";
    std::cout << "  올바른 순서: 파생(out_of_range) → 기반(exception)\n";
    try
    {
        throw std::out_of_range("index 999");
    }
    catch (const std::out_of_range& e)
    {
        // 파생 클래스를 먼저 잡아야 정확한 타입을 알 수 있음
        std::cout << "  [CAUGHT] 파생 클래스 catch: std::out_of_range\n";
    }
    catch (const std::exception& e)
    {
        // 이 줄은 실행되지 않음 (위에서 이미 잡힘)
        std::cout << "  [CAUGHT] 기반 클래스 catch: std::exception\n";
    }

    std::cout << "\n  [주의] 만약 기반(std::exception)을 먼저 쓰면\n";
    std::cout << "  모든 예외가 기반 catch에 잡혀서 타입 구분이 안 됩니다!\n";

    std::cout << "\n  ✓ A 완료 - 모든 예외를 잡아서 프로그램이 정상 실행 중입니다.\n";
}

// ============================================================================
// B: 함수 경계에서 예외를 에러코드로 변환
// ============================================================================

// 시뮬레이션: 외부 라이브러리 함수 (예외를 던질 수 있음)
namespace ThirdPartyLib
{
    struct ParseResult
    {
        std::string name;
        int         hp = 0;
        float       speed = 0.0f;
    };

    // 이 함수는 예외를 던질 수 있음 (외부 라이브러리라 제어 불가)
    ParseResult ParseConfig(const std::string& data)
    {
        if (data.empty())
            throw std::invalid_argument("Empty config data");

        if (data.find('{') == std::string::npos)
            throw std::runtime_error("Invalid JSON format");

        // 정상 파싱 시뮬레이션
        return {"Hero", 100, 5.5f};
    }
}

// 래퍼 함수: 예외를 에러코드로 변환하는 경계 함수
bool SafeParseConfig(const std::string& data, ThirdPartyLib::ParseResult& outResult)
{
    try
    {
        outResult = ThirdPartyLib::ParseConfig(data);
        return true;  // 성공
    }
    catch (const std::invalid_argument& e)
    {
        std::cout << "  [WARNING] 파싱 실패 (invalid_argument): " << e.what() << "\n";
        return false;
    }
    catch (const std::runtime_error& e)
    {
        std::cout << "  [WARNING] 파싱 실패 (runtime_error): " << e.what() << "\n";
        return false;
    }
    catch (...)
    {
        std::cout << "  [WARNING] 파싱 실패 (알 수 없는 예외)\n";
        return false;
    }
    // 이 함수 바깥으로는 예외가 절대 나가지 않음!
}

void HandleB_ExceptionToErrorCode()
{
    std::cout << "\n[B] 함수 경계에서 예외를 에러코드로 변환\n";
    std::cout << "============================================================\n";
    std::cout << "  핵심: try-catch는 외부 라이브러리와의 '경계'에서만 사용!\n";
    std::cout << "  예외를 bool 리턴값으로 변환하여 엔진 내부로 전달합니다.\n\n";

    ThirdPartyLib::ParseResult result;

    // 1) 정상 케이스
    std::cout << "  --- 1) 정상 데이터 ---\n";
    if (SafeParseConfig("{name:Hero}", result))
    {
        std::cout << "  [OK] 파싱 성공: " << result.name
                  << " (HP:" << result.hp << ")\n";
    }

    // 2) 빈 데이터 → invalid_argument 발생 → false 리턴
    std::cout << "\n  --- 2) 빈 데이터 (invalid_argument 발생) ---\n";
    if (!SafeParseConfig("", result))
    {
        std::cout << "  → 에러코드(false)로 변환됨. 프로그램 계속 실행!\n";
    }

    // 3) 잘못된 포맷 → runtime_error 발생 → false 리턴
    std::cout << "\n  --- 3) 잘못된 포맷 (runtime_error 발생) ---\n";
    if (!SafeParseConfig("invalid data", result))
    {
        std::cout << "  → 에러코드(false)로 변환됨. 프로그램 계속 실행!\n";
    }

    std::cout << "\n  [포인트] 게임 엔진 내부에서는 if 체크로 에러를 처리하고,\n";
    std::cout << "  try-catch는 외부 라이브러리 호출 경계에서만 사용합니다.\n";

    std::cout << "\n  ✓ B 완료\n";
}

// ============================================================================
// C: 생성자 실패와 Function Try Block
// ============================================================================

// --- 문제 상황: 생성자에서 예외 발생 시 소멸자 미호출 ---
class UnsafeResource
{
    int* m_Data;
    int* m_Extra;

public:
    UnsafeResource()
    {
        std::cout << "  [생성자] 리소스 1 할당\n";
        m_Data = new int[100];

        std::cout << "  [생성자] 리소스 2 할당 중 예외 발생!\n";
        throw std::runtime_error("리소스 2 초기화 실패");
        // m_Data는 누가 해제하지? → 소멸자가 안 불림! → 릭!
    }

    ~UnsafeResource()
    {
        std::cout << "  [소멸자] 호출됨 - 리소스 해제\n";
        delete[] m_Data;
    }
};

// --- 해결 1: Function Try Block ---
class FunctionTryBlockDemo
{
    int* m_Data;

public:
    FunctionTryBlockDemo()
    try
        : m_Data(new int[100])   // 초기화 리스트에서도 예외를 잡을 수 있음
    {
        std::cout << "  [생성자] 리소스 할당 성공\n";
        std::cout << "  [생성자] 추가 초기화 중 예외 발생 시뮬레이션\n";
        throw std::runtime_error("초기화 실패");
    }
    catch (const std::exception& e)
    {
        std::cout << "  [Function Try Block] 예외 잡음: " << e.what() << "\n";
        std::cout << "  [Function Try Block] 리소스 정리: m_Data 해제\n";
        delete[] m_Data;
        // 주의: 여기서 블록이 끝나면 자동으로 rethrow 됨!
        // 생성 실패한 객체를 정상인 척 할 수 없기 때문.
        std::cout << "  [Function Try Block] catch 블록 끝 → 자동 rethrow!\n";
    }

    ~FunctionTryBlockDemo()
    {
        std::cout << "  [소멸자] 호출됨\n";
        delete[] m_Data;
    }
};

// --- 해결 2: Init() 분리 패턴 (게임 엔진 스타일) ---
class SafeResource
{
    int* m_Data  = nullptr;
    int* m_Extra = nullptr;

public:
    SafeResource() = default;  // 생성자는 절대 실패하지 않음

    bool Init()
    {
        m_Data = new(std::nothrow) int[100];
        if (!m_Data)
        {
            std::cout << "  [Init] 리소스 1 할당 실패\n";
            return false;
        }
        std::cout << "  [Init] 리소스 1 할당 성공\n";

        // 시뮬레이션: 리소스 2 실패
        std::cout << "  [Init] 리소스 2 할당 실패 시뮬레이션\n";
        delete[] m_Data;
        m_Data = nullptr;
        return false;  // 실패를 리턴값으로 알림 (예외 없음!)
    }

    ~SafeResource()
    {
        // nullptr 체크 후 안전하게 해제
        if (m_Data)  { delete[] m_Data;  std::cout << "  [소멸자] m_Data 해제\n"; }
        if (m_Extra) { delete[] m_Extra; std::cout << "  [소멸자] m_Extra 해제\n"; }
        std::cout << "  [소멸자] 정상 호출됨\n";
    }
};

void HandleC_ConstructorException()
{
    std::cout << "\n[C] 생성자 실패와 Function Try Block\n";
    std::cout << "============================================================\n";
    std::cout << "  생성자는 리턴값이 없으므로, 실패를 알리는 방법이 제한적입니다.\n\n";

    // 1) 문제: 생성자에서 예외 → 소멸자 미호출
    std::cout << "  --- 1) 문제: 생성자 예외 시 소멸자 미호출 ---\n";
    try
    {
        UnsafeResource res;  // 생성자에서 throw
    }
    catch (const std::exception& e)
    {
        std::cout << "  [CAUGHT] " << e.what() << "\n";
        std::cout << "  → 소멸자가 호출되지 않았습니다! m_Data 메모리 릭!\n";
    }

    // 2) 해결 1: Function Try Block
    std::cout << "\n  --- 2) 해결: Function Try Block ---\n";
    try
    {
        FunctionTryBlockDemo demo;  // Function Try Block에서 정리 후 rethrow
    }
    catch (const std::exception& e)
    {
        std::cout << "  [CAUGHT] rethrow된 예외: " << e.what() << "\n";
        std::cout << "  → Function Try Block 안에서 리소스를 직접 정리했습니다.\n";
    }

    // 3) 해결 2: Init() 분리 (게임 엔진 스타일)
    std::cout << "\n  --- 3) 게임 엔진 스타일: Init() 분리 ---\n";
    {
        SafeResource res;
        if (!res.Init())
        {
            std::cout << "  [FAIL] Init 실패 → 에러코드(false)로 처리.\n";
            std::cout << "  → 예외 없이 실패를 처리! 소멸자도 정상 호출됨.\n";
        }
    }  // 스코프 끝 → 소멸자 호출

    std::cout << "\n  [결론] 게임 엔진에서는 Init() 분리 패턴을 선호합니다.\n";
    std::cout << "  생성자는 실패하지 않는 기본 초기화만, 위험한 작업은 Init()에서.\n";

    std::cout << "\n  ✓ C 완료\n";
}

// ============================================================================
// D: __try/__except로 Access Violation 잡기
// ============================================================================

// 주의: SEH 핸들러가 있는 함수에는 C++ 객체의 unwind가 필요한
// 코드를 넣을 수 없으므로 (/EHa에서도 별도 함수로 분리 권장)
void CrashFunction_NullDeref()
{
    int* p = nullptr;
    *p = 42;  // Access Violation!
}

void HandleD_SehBasic()
{
    std::cout << "\n[D] __try/__except로 Access Violation 잡기\n";
    std::cout << "============================================================\n";
    std::cout << "  SEH는 Windows OS 레벨의 예외 처리 메커니즘입니다.\n";
    std::cout << "  C++ try-catch로 잡을 수 없는 하드웨어 예외를 잡습니다.\n\n";

    // 1) nullptr 역참조 잡기
    std::cout << "  --- 1) nullptr 역참조 (Access Violation) ---\n";
    __try
    {
        std::cout << "  nullptr에 쓰기 시도...\n";
        CrashFunction_NullDeref();
        std::cout << "  이 줄은 실행되지 않습니다.\n";
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        std::cout << "  [SEH CAUGHT] Access Violation을 잡았습니다!\n";
        std::cout << "  예외 코드: 0x" << std::hex << GetExceptionCode() << std::dec << "\n";
        std::cout << "  → C++ try-catch로는 불가능한 일입니다.\n";
    }

    // 2) 0 나누기 잡기
    std::cout << "\n  --- 2) 정수 0 나누기 ---\n";
    __try
    {
        volatile int a = 100;
        volatile int b = 0;
        std::cout << "  100 / 0 계산 시도...\n";
        volatile int result = a / b;  // INT_DIVIDE_BY_ZERO
        std::cout << "  result = " << result << "\n";
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        std::cout << "  [SEH CAUGHT] Division by Zero를 잡았습니다!\n";
        std::cout << "  예외 코드: 0x" << std::hex << GetExceptionCode() << std::dec << "\n";
    }

    std::cout << "\n  [포인트] __try/__except는 Windows 전용이며,\n";
    std::cout << "  주로 최상위 레벨에서 크래시 수집용으로 사용합니다.\n";

    std::cout << "\n  ✓ D 완료 - 하드웨어 예외를 잡아서 프로그램이 살아있습니다.\n";
}

// ============================================================================
// E: SEH 필터 함수로 예외 코드 분류
// ============================================================================

// SEH 필터 함수: 예외 종류에 따라 처리 여부를 결정
LONG WINAPI ExceptionFilter(EXCEPTION_POINTERS* pExInfo)
{
    DWORD code = pExInfo->ExceptionRecord->ExceptionCode;

    // 크래시 원인 메시지 출력
    PrintCrashMessage(pExInfo);

    switch (code)
    {
    case EXCEPTION_ACCESS_VIOLATION:
    case EXCEPTION_INT_DIVIDE_BY_ZERO:
    case EXCEPTION_STACK_OVERFLOW:
        std::cout << "  [필터] → EXCEPTION_EXECUTE_HANDLER (잡겠다!)\n";
        return EXCEPTION_EXECUTE_HANDLER;

    default:
        std::cout << "  [필터] → EXCEPTION_CONTINUE_SEARCH (상위로 전달)\n";
        return EXCEPTION_CONTINUE_SEARCH;
    }
}

void CrashFunction_DivZero()
{
    volatile int a = 42;
    volatile int b = 0;
    volatile int c = a / b;
}

void HandleE_SehFilter()
{
    std::cout << "\n[E] SEH 필터 함수로 예외 코드 분류\n";
    std::cout << "============================================================\n";
    std::cout << "  필터 함수에서 예외 종류를 분석하고 처리 여부를 결정합니다.\n";
    std::cout << "  EXCEPTION_EXECUTE_HANDLER: 잡겠다 (→ __except 블록 실행)\n";
    std::cout << "  EXCEPTION_CONTINUE_SEARCH: 못 잡겠다 (→ 상위 핸들러로)\n\n";

    // 1) Access Violation
    std::cout << "  --- 1) Access Violation ---\n";
    __try
    {
        CrashFunction_NullDeref();
    }
    __except (ExceptionFilter(GetExceptionInformation()))
    {
        std::cout << "  [HANDLED] 필터 함수가 분석 후 처리 완료.\n\n";
    }

    // 2) Division by Zero
    std::cout << "  --- 2) Division by Zero ---\n";
    __try
    {
        CrashFunction_DivZero();
    }
    __except (ExceptionFilter(GetExceptionInformation()))
    {
        std::cout << "  [HANDLED] 필터 함수가 분석 후 처리 완료.\n";
    }

    std::cout << "\n  [포인트] 필터 함수에서 예외 정보(EXCEPTION_POINTERS)를 분석하면\n";
    std::cout << "  읽기/쓰기, 주소, 예외 종류 등을 상세히 알 수 있습니다.\n";

    std::cout << "\n  ✓ E 완료\n";
}

// ============================================================================
// F: SEH + 미니덤프(.dmp) 파일 생성
// ============================================================================

// 미니덤프를 생성하는 SEH 필터
LONG WINAPI DumpFilter(EXCEPTION_POINTERS* pExInfo)
{
    std::cout << "\n";

    // MessageBox로 크래시 정보 표시 + 덤프 저장 여부 확인
    bool bSaveDump = PrintCrashMessage(pExInfo, true);

    if (bSaveDump)
    {
        // 덤프 파일명에 Git revision 포함 → 파일명만 봐도 어떤 빌드인지 알 수 있음
        // 브랜치명의 '/' 등을 '_'로 치환 (파일명에 쓸 수 없는 문자 방지)
        char safeBranch[128];
        MakeSafeFilename(safeBranch, sizeof(safeBranch), BUILD_GIT_BRANCH);

        wchar_t dumpFile[256];
        swprintf_s(dumpFile, L"CrashDump_%hs_%hs.dmp",
            BUILD_GIT_REVISION, safeBranch);

        char dumpFileA[256];
        sprintf_s(dumpFileA, "CrashDump_%s_%s.dmp",
            BUILD_GIT_REVISION, safeBranch);

        std::cout << "  [덤프 필터] 미니덤프 생성 중: " << dumpFileA << "\n";

        if (WriteMiniDump(pExInfo, dumpFile))
        {
            std::cout << "  [덤프 필터] ✓ 미니덤프 생성 성공!\n";
            std::cout << "  [덤프 필터] Visual Studio에서 .dmp 파일을 열면\n";
            std::cout << "             크래시 시점의 콜스택을 확인할 수 있습니다.\n";
            std::cout << "  [덤프 필터] 방법: .dmp 더블클릭 → '네이티브만 디버깅'\n";
            std::cout << "  [덤프 필터] 조사식에 g_BuildGitRevision 입력 → revision 확인!\n";
        }
        else
        {
            std::cout << "  [덤프 필터] ✗ 미니덤프 생성 실패!\n";
        }
    }
    else
    {
        std::cout << "  [덤프 필터] 사용자가 덤프 저장을 취소했습니다.\n";
    }

    return EXCEPTION_EXECUTE_HANDLER;
}

void HandleF_SehWithDump()
{
    std::cout << "\n[F] SEH + 미니덤프(.dmp) 파일 생성\n";
    std::cout << "============================================================\n";
    std::cout << "  크래시 발생 시 미니덤프를 남기면 나중에 원인을 분석할 수 있습니다.\n";
    std::cout << "  MiniDumpWriteDump() API를 사용합니다. (DbgHelp.h)\n\n";

    __try
    {
        std::cout << "  nullptr 역참조로 크래시 유발...\n";
        CrashFunction_NullDeref();
    }
    __except (DumpFilter(GetExceptionInformation()))
    {
        std::cout << "\n  [HANDLED] 크래시를 잡고 덤프를 남겼습니다.\n";
        std::cout << "  프로그램은 계속 실행됩니다.\n";

        // 덤프 파일 존재 확인
        char safeBranchCheck[128];
        MakeSafeFilename(safeBranchCheck, sizeof(safeBranchCheck), BUILD_GIT_BRANCH);

        wchar_t checkFile[256];
        swprintf_s(checkFile, L"CrashDump_%hs_%hs.dmp",
            BUILD_GIT_REVISION, safeBranchCheck);

        DWORD attr = GetFileAttributesW(checkFile);
        if (attr != INVALID_FILE_ATTRIBUTES)
        {
            WIN32_FILE_ATTRIBUTE_DATA fileInfo;
            GetFileAttributesExW(checkFile, GetFileExInfoStandard, &fileInfo);
            DWORD fileSize = fileInfo.nFileSizeLow;
            std::cout << "\n  [확인] 덤프 파일 크기: "
                      << fileSize / 1024 << " KB\n";
            std::cout << "  [확인] 실행 파일과 같은 폴더에 생성되었습니다.\n";
        }
    }

    char safeBranchName[128];
    MakeSafeFilename(safeBranchName, sizeof(safeBranchName), BUILD_GIT_BRANCH);
    char dumpNameA[256];
    sprintf_s(dumpNameA, "CrashDump_%s_%s.dmp", BUILD_GIT_REVISION, safeBranchName);

    std::cout << "\n  [실습] 생성된 .dmp 파일을 Visual Studio에서 열어보세요!\n";
    std::cout << "  1. " << dumpNameA << " 파일을 더블클릭\n";
    std::cout << "  2. '네이티브만 사용하여 디버깅' 클릭\n";
    std::cout << "  3. 콜스택에서 CrashFunction_NullDeref()를 확인\n";
    std::cout << "  4. 조사식(Watch)에 g_BuildGitRevision 입력 → Git revision 확인!\n";

    std::cout << "\n  ✓ F 완료\n";
}

// ============================================================================
// G: 게임 메인루프 보호 패턴
// ============================================================================

// 간단한 Assert 매크로 (게임 엔진 스타일)
#define GAME_CHECK(expr) \
    do { \
        if (!(expr)) { \
            std::cout << "  [ASSERT FAILED] " << #expr << "\n"; \
            std::cout << "  File: " << __FILE__ << "\n"; \
            std::cout << "  Line: " << __LINE__ << "\n"; \
            __debugbreak(); /* SEH EXCEPTION_BREAKPOINT 발생 */ \
        } \
    } while(0)

// 간이 게임 시뮬레이션
namespace GameSim
{
    struct Entity
    {
        std::string name;
        int hp = 100;
    };

    // 매 프레임 업데이트 (일부러 특정 프레임에서 크래시)
    void UpdateFrame(int frame, Entity* player)
    {
        if (frame < 3)
        {
            std::cout << "  [Frame " << frame << "] 정상 업데이트. HP=" << player->hp << "\n";
            player->hp -= 10;
        }
        else
        {
            // 프레임 3에서 의도적 크래시
            std::cout << "  [Frame " << frame << "] 버그 발생! nullptr 접근!\n";
            int* bad = nullptr;
            *bad = 999;  // Access Violation
        }
    }

    // 덤프를 남기는 SEH 필터
    LONG WINAPI GameCrashFilter(EXCEPTION_POINTERS* pExInfo)
    {
        std::cout << "\n  ╔══════════════════════════════════════════╗\n";
        std::cout << "  ║       GAME CRASH DETECTED!               ║\n";
        std::cout << "  ╚══════════════════════════════════════════╝\n";

        // MessageBox로 크래시 정보 표시 + 덤프 저장 여부 확인
        bool bSaveDump = PrintCrashMessage(pExInfo, true);

        if (bSaveDump)
        {
            // 미니덤프 생성 (파일명에 revision 포함)
            char safeBranch[128];
            MakeSafeFilename(safeBranch, sizeof(safeBranch), BUILD_GIT_BRANCH);

            wchar_t dumpFile[256];
            swprintf_s(dumpFile, L"GameCrash_%hs_%hs.dmp",
                BUILD_GIT_REVISION, safeBranch);

            char dumpFileA[256];
            sprintf_s(dumpFileA, "GameCrash_%s_%s.dmp",
                BUILD_GIT_REVISION, safeBranch);

            if (WriteMiniDump(pExInfo, dumpFile))
            {
                std::cout << "  Crash dump saved: " << dumpFileA << "\n";
            }
            else
            {
                std::cout << "  Crash dump 생성 실패!\n";
            }
        }
        else
        {
            std::cout << "  사용자가 덤프 저장을 취소했습니다.\n";
        }

        return EXCEPTION_EXECUTE_HANDLER;
    }
}

// SEH __try는 C++ 소멸자가 있는 객체와 같은 함수에서 사용할 수 없음 (C2712)
// → __try 부분을 별도 함수로 분리하는 것이 실전에서도 쓰는 패턴입니다.
void RunGameLoop_Protected(GameSim::Entity* player)
{
    __try
    {
        for (int frame = 0; frame < 5; ++frame)
        {
            GameSim::UpdateFrame(frame, player);
        }
        std::cout << "  === 게임 루프 정상 종료 ===\n";
    }
    __except (GameSim::GameCrashFilter(GetExceptionInformation()))
    {
        std::cout << "\n  [RECOVERY] 게임 루프에서 크래시 발생!\n";
        std::cout << "  [RECOVERY] 덤프를 남기고 안전하게 종료합니다.\n";
        std::cout << "  [RECOVERY] 실제 게임에서는 여기서:\n";
        std::cout << "    - 자동 저장 시도\n";
        std::cout << "    - 크래시 리포트 전송\n";
        std::cout << "    - 사용자에게 오류 메시지 표시\n";
    }
}

void HandleG_GameMainLoopProtection()
{
    std::cout << "\n[G] 게임 메인루프 보호 패턴\n";
    std::cout << "============================================================\n";
    std::cout << "  실제 게임에서 사용하는 패턴:\n";
    std::cout << "  - 최상위: SEH로 감싸서 크래시 시 덤프 저장\n";
    std::cout << "  - 내부: Assert(check)로 조건 검증\n";
    std::cout << "  - C++ try-catch: 사용하지 않음\n\n";

    std::cout << "  [참고] MSVC에서 __try는 C++ 소멸자가 있는 객체와\n";
    std::cout << "  같은 함수에서 사용할 수 없습니다. (에러 C2712)\n";
    std::cout << "  → __try 부분을 별도 함수로 분리하는 것이 실전 패턴입니다.\n\n";

    GameSim::Entity player;
    player.name = "Hero";
    player.hp = 100;

    std::cout << "  === 게임 루프 시작 (5프레임 실행 예정) ===\n\n";

    // SEH 보호 함수를 별도로 호출 (C2712 회피 패턴)
    RunGameLoop_Protected(&player);

    std::cout << "\n  ✓ G 완료\n";
}

// ============================================================================
// H: 서드파티 라이브러리 경계 패턴
// ============================================================================

// 시뮬레이션: 외부 물리 엔진 (우리가 소스코드를 못 바꿈)
namespace ExternalPhysicsLib
{
    struct RaycastHit
    {
        float distance = 0.0f;
        float normal[3] = {0, 0, 0};
    };

    // 이 함수들은 실패 시 예외를 던짐 (외부 라이브러리 설계)
    void Initialize(int maxObjects)
    {
        if (maxObjects <= 0)
            throw std::invalid_argument("maxObjects must be > 0");
        if (maxObjects > 100000)
            throw std::runtime_error("Too many objects for physics simulation");
        std::cout << "    [PhysicsLib] Initialized with " << maxObjects << " objects\n";
    }

    RaycastHit Raycast(float originX, float originY, float originZ)
    {
        // NaN 좌표 체크
        if (originX != originX)  // NaN 체크 (NaN != NaN은 true)
            throw std::runtime_error("NaN detected in raycast origin");
        return {10.5f, {0.0f, 1.0f, 0.0f}};
    }

    void Shutdown()
    {
        std::cout << "    [PhysicsLib] Shutdown complete\n";
    }
}

// 게임 엔진의 물리 래퍼 (경계 계층)
class PhysicsWrapper
{
public:
    bool Init(int maxObjects)
    {
        try
        {
            ExternalPhysicsLib::Initialize(maxObjects);
            std::cout << "    [Wrapper] 물리 엔진 초기화 성공\n";
            return true;
        }
        catch (const std::exception& e)
        {
            std::cout << "    [Wrapper] 물리 엔진 초기화 실패: " << e.what() << "\n";
            return false;  // 예외 → 에러코드 변환
        }
    }

    bool DoRaycast(float x, float y, float z, ExternalPhysicsLib::RaycastHit& outHit)
    {
        try
        {
            outHit = ExternalPhysicsLib::Raycast(x, y, z);
            return true;
        }
        catch (const std::exception& e)
        {
            std::cout << "    [Wrapper] Raycast 실패: " << e.what() << "\n";
            outHit = {};
            return false;
        }
    }

    void Cleanup()
    {
        try
        {
            ExternalPhysicsLib::Shutdown();
        }
        catch (...) { /* 종료 시 예외 무시 */ }
    }
};

void HandleH_ThirdPartyBoundary()
{
    std::cout << "\n[H] 서드파티 라이브러리 경계 패턴\n";
    std::cout << "============================================================\n";
    std::cout << "  외부 라이브러리가 예외를 던지면, 래퍼(Wrapper)에서\n";
    std::cout << "  try-catch로 잡아서 에러코드로 변환합니다.\n";
    std::cout << "  게임 엔진 내부로는 예외가 절대 전파되지 않습니다.\n\n";

    PhysicsWrapper physics;

    // 1) 정상 초기화
    std::cout << "  --- 1) 정상 초기화 ---\n";
    if (physics.Init(1000))
    {
        std::cout << "  [게임] 물리 엔진 사용 준비 완료\n";
    }

    // 2) 잘못된 파라미터로 초기화 → 예외 발생 → false 리턴
    std::cout << "\n  --- 2) 잘못된 파라미터 (-1) ---\n";
    PhysicsWrapper physics2;
    if (!physics2.Init(-1))
    {
        std::cout << "  [게임] 초기화 실패 → 대체 물리 사용 또는 기능 비활성화\n";
    }

    // 3) 과도한 오브젝트 수 → 예외 발생 → false 리턴
    std::cout << "\n  --- 3) 과도한 오브젝트 수 (999999) ---\n";
    PhysicsWrapper physics3;
    if (!physics3.Init(999999))
    {
        std::cout << "  [게임] 초기화 실패 → 에러 메시지 표시\n";
    }

    // 4) Raycast 정상
    std::cout << "\n  --- 4) 정상 Raycast ---\n";
    ExternalPhysicsLib::RaycastHit hit;
    if (physics.DoRaycast(0.0f, 1.0f, 0.0f, hit))
    {
        std::cout << "  [게임] Raycast 성공: 거리=" << hit.distance << "\n";
    }

    // 5) NaN 좌표로 Raycast → 예외 발생 → false 리턴
    std::cout << "\n  --- 5) NaN 좌표 Raycast ---\n";
    float nan = std::numeric_limits<float>::quiet_NaN();
    if (!physics.DoRaycast(nan, 0.0f, 0.0f, hit))
    {
        std::cout << "  [게임] Raycast 실패 → 기본값 사용\n";
    }

    physics.Cleanup();

    std::cout << "\n  [구조도]\n";
    std::cout << "  ┌────────────────────────────────────┐\n";
    std::cout << "  │  게임 엔진 내부 (예외 없음)           │\n";
    std::cout << "  │    if (!physics.Init(...))          │\n";
    std::cout << "  │        HandleError();               │\n";
    std::cout << "  │  ┌──────────────────────────────┐  │\n";
    std::cout << "  │  │  PhysicsWrapper (try-catch)   │  │\n";
    std::cout << "  │  │  예외 → bool 변환             │  │\n";
    std::cout << "  │  │  ┌──────────────────────┐    │  │\n";
    std::cout << "  │  │  │ ExternalPhysicsLib    │    │  │\n";
    std::cout << "  │  │  │ (throw 가능)          │    │  │\n";
    std::cout << "  │  │  └──────────────────────┘    │  │\n";
    std::cout << "  │  └──────────────────────────────┘  │\n";
    std::cout << "  └────────────────────────────────────┘\n";

    std::cout << "\n  ✓ H 완료\n";
}

// ============================================================================
// 메인
// ============================================================================
void PrintMenu()
{
    std::cout << "\n====================================================\n";
    std::cout << "  ZeroCrashLab - 12. Exception Handling\n";
    std::cout << "  (예외 처리 방법)\n";
    std::cout << "  Build: " << BUILD_GIT_REVISION << " (" << BUILD_GIT_BRANCH << ")\n";
    std::cout << "  Built: " << BUILD_TIMESTAMP << "\n";
    std::cout << "====================================================\n";
    std::cout << "\n  이 프로그램은 예외를 '처리'하는 다양한 방법을 시연합니다.\n";
    std::cout << "  (11번과 달리 크래시하지 않고 정상 실행됩니다)\n";
    std::cout << "\n";
    std::cout << "  ──── C++ Exception Handling ────\n";
    std::cout << "  [A] try-catch 기본       (std::exception 계열 잡기)\n";
    std::cout << "  [B] 예외→에러코드 변환   (함수 경계 패턴)\n";
    std::cout << "  [C] 생성자 예외 처리     (Function Try Block)\n";
    std::cout << "\n";
    std::cout << "  ──── SEH (Structured Exception Handling) ────\n";
    std::cout << "  [D] __try/__except 기본  (하드웨어 예외 잡기)\n";
    std::cout << "  [E] SEH 필터 함수        (예외 코드 분류)\n";
    std::cout << "  [F] SEH + 미니덤프       (.dmp 파일 생성)\n";
    std::cout << "\n";
    std::cout << "  ──── 실전 패턴 ────\n";
    std::cout << "  [G] 게임 메인루프 보호   (SEH 최상위 + Assert)\n";
    std::cout << "  [H] 서드파티 경계 패턴   (try-catch → 에러코드)\n";
    std::cout << "\n";
    std::cout << "  [M] 메뉴 다시 보기\n";
    std::cout << "  [Q] 종료\n";
    std::cout << "----------------------------------------------------\n";
}

int main()
{
    PrintMenu();

    while (true)
    {
        std::cout << "\n선택> ";
        char choice;
        std::cin >> choice;
        choice = toupper(choice);

        switch (choice)
        {
        // C++ Exception Handling
        case 'A': HandleA_TryCatchBasic(); break;
        case 'B': HandleB_ExceptionToErrorCode(); break;
        case 'C': HandleC_ConstructorException(); break;

        // SEH
        case 'D': HandleD_SehBasic(); break;
        case 'E': HandleE_SehFilter(); break;
        case 'F': HandleF_SehWithDump(); break;

        // 실전 패턴
        case 'G': HandleG_GameMainLoopProtection(); break;
        case 'H': HandleH_ThirdPartyBoundary(); break;

        case 'M': PrintMenu(); break;
        case 'Q': std::cout << "종료합니다.\n"; return 0;
        default:  std::cout << "잘못된 입력입니다. (M: 메뉴 보기)\n"; break;
        }
    }
}
