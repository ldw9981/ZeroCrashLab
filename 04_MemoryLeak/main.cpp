/*============================================================================
 *  ZeroCrashLab - 04. Memory Leak (메모리 누수)
 *  ---------------------------------------------------------------------------
 *  new로 할당한 메모리를 delete하지 않으면 메모리가 누적되어
 *  결국 OOM(Out of Memory) 크래시 또는 성능 저하를 유발합니다.
 *
 *  [교육 목표] 메모리가 누수되는 패턴을 찾고, RAII/스마트 포인터로 수정하세요.
 *  이 프로그램은 크래시 대신 메모리 사용량 증가를 눈으로 보여줍니다.
 *============================================================================*/
#include <iostream>
#include <string>
#include <vector>
#include <cstdlib>

// ============================================================================
// 간이 클래스들
// ============================================================================

class IState {
public:
    virtual ~IState() = default;
    virtual std::string GetName() const = 0;
};

class IdleState : public IState {
    char padding[1024];  // 메모리 누수를 눈에 띄게 하기 위한 패딩
public:
    std::string GetName() const override { return "Idle"; }
};

class WalkState : public IState {
    char padding[1024];
public:
    std::string GetName() const override { return "Walk"; }
};

class RunState : public IState {
    char padding[1024];
public:
    std::string GetName() const override { return "Run"; }
};

class AttackState : public IState {
    char padding[1024];
public:
    std::string GetName() const override { return "Attack"; }
};

// 다형적 인터페이스 (가상 소멸자 누락!)
class IRenderer {
public:
    // BAD: virtual ~IRenderer() = default; 가 없음!
    virtual void Render() = 0;
};

class MeshRenderer : public IRenderer {
    std::vector<float> meshData;
public:
    MeshRenderer() : meshData(10000, 1.0f) {}  // 40KB 메시 데이터
    ~MeshRenderer() { std::cout << "    ~MeshRenderer() 호출됨\n"; }
    void Render() override { /* ... */ }
};

// ============================================================================
// BUG A: 소멸자 호출만 하고 메모리 해제 안 함
// ============================================================================
struct ObjectSlot {
    IState* ptr = nullptr;
};

void BugA_DestructorWithoutDelete() {
    std::cout << "\n[BUG A] 소멸자 호출만 하고 메모리 해제 안 함\n";
    std::cout << "  new로 할당 후 소멸자만 호출하면 메모리는 해제되지 않습니다.\n\n";

    const int COUNT = 1000;
    std::cout << "  " << COUNT << "개 객체를 생성하고 소멸자만 호출합니다...\n";

    for (int i = 0; i < COUNT; i++) {
        ObjectSlot slot;
        slot.ptr = new IdleState();

        // BAD: 소멸자만 호출, 메모리는 해제 안 됨!
        slot.ptr->~IState();
        slot.ptr = nullptr;
        // delete slot.ptr; 이 빠져있음!
    }

    std::cout << "  [결과] " << COUNT << "KB 이상의 메모리가 누수되었습니다!\n";
    std::cout << "  (각 객체 1KB x " << COUNT << " = 약 " << COUNT << "KB)\n";
}

// ============================================================================
// BUG B: FSM 상태 객체 new 후 미해제
// ============================================================================
class PlayerController {
public:
    IState* fsmStates[4];
    IState* curState;

    void Init() {
        // BAD: new로 생성하지만 소멸자에서 delete 없음!
        fsmStates[0] = new IdleState();
        fsmStates[1] = new WalkState();
        fsmStates[2] = new RunState();
        fsmStates[3] = new AttackState();
        curState = fsmStates[0];
    }

    void Update() {
        std::cout << "    현재 상태: " << curState->GetName() << "\n";
    }

    // BAD: 소멸자에서 fsmStates를 delete하지 않음!
    ~PlayerController() {
        std::cout << "    ~PlayerController() - fsmStates delete 없음!\n";
    }
};

void BugB_FSMStateLeaks() {
    std::cout << "\n[BUG B] FSM 상태 객체 new 후 미해제\n";
    std::cout << "  상태 패턴에서 new로 생성한 상태 객체를 delete하지 않습니다.\n\n";

    const int ITERATIONS = 500;
    std::cout << "  " << ITERATIONS << "번 PlayerController 생성/파괴를 반복합니다...\n";

    for (int i = 0; i < ITERATIONS; i++) {
        PlayerController* pc = new PlayerController();
        pc->Init();
        pc->Update();
        delete pc;  // ~PlayerController()에서 fsmStates를 delete하지 않음!
    }

    std::cout << "\n  [결과] " << ITERATIONS * 4 << "개의 State 객체가 누수!\n";
    std::cout << "  (한 번에 4개 상태 x " << ITERATIONS << "회 = "
              << ITERATIONS * 4 << "개, 약 " << ITERATIONS * 4 << "KB)\n";
}

// ============================================================================
// BUG C: 가상 소멸자 누락
// ============================================================================
void BugC_MissingVirtualDestructor() {
    std::cout << "\n[BUG C] 가상 소멸자 누락\n";
    std::cout << "  다형적 인터페이스에 virtual 소멸자가 없으면\n";
    std::cout << "  파생 클래스의 소멸자가 호출되지 않습니다.\n\n";

    std::cout << "  IRenderer* = new MeshRenderer() 생성...\n";
    IRenderer* renderer = new MeshRenderer();
    renderer->Render();

    std::cout << "  delete renderer 호출...\n";
    // BAD: IRenderer에 virtual 소멸자가 없으므로 ~MeshRenderer()가 호출되지 않음!
    delete renderer;
    // ~MeshRenderer()가 호출되지 않으면 meshData(40KB)가 누수!

    std::cout << "  [결과] ~MeshRenderer()가 호출되지 않았다면 메모리 누수!\n";
    std::cout << "  (virtual ~IRenderer() = default; 가 없기 때문)\n";
}

// ============================================================================
// BUG D: 캐시 없이 반복 로딩
// ============================================================================
struct FBXAsset {
    std::string path;
    std::vector<float> vertexData;
    FBXAsset(const std::string& p) : path(p), vertexData(50000, 0.0f) {
        // 약 200KB의 정점 데이터
    }
};

FBXAsset* LoadAsset(const std::string& path) {
    // BAD: 캐시 검색 없이 매번 새로 로드!
    auto* asset = new FBXAsset(path);
    return asset;
    // 이전에 로드한 동일 경로의 asset은 해제 없이 방치 → 누수
}

void BugD_RepeatedLoadingWithoutCache() {
    std::cout << "\n[BUG D] 캐시 없이 반복 로딩 (메모리 누수)\n";
    std::cout << "  같은 리소스를 매번 new로 로드하면 이전 것은 누수됩니다.\n\n";

    const int LOADS = 100;
    FBXAsset* currentAsset = nullptr;

    for (int i = 0; i < LOADS; i++) {
        // BAD: 이전 asset을 delete하지 않고 새로 로드!
        currentAsset = LoadAsset("models/character.fbx");
    }

    std::cout << "  같은 파일을 " << LOADS << "번 로드했습니다.\n";
    std::cout << "  [결과] " << (LOADS - 1) << "개의 asset이 누수! (약 "
              << (LOADS - 1) * 200 << "KB)\n";
    std::cout << "  마지막 것만 접근 가능: " << currentAsset->path << "\n";

    delete currentAsset;
}

// ============================================================================
// 메인
// ============================================================================
int main() {
    std::cout << "====================================================\n";
    std::cout << "  ZeroCrashLab - 04. Memory Leak\n";
    std::cout << "  (메모리 누수)\n";
    std::cout << "====================================================\n";
    std::cout << "\n  이 프로그램은 메모리 누수를 시연합니다.\n";
    std::cout << "  크래시 대신 메모리 사용량 증가를 확인하세요.\n";
    std::cout << "  교육생은 누수 원인을 파악하고 코드를 수정하세요.\n\n";
    std::cout << "  [A] 소멸자 호출만 하고 메모리 해제 안 함\n";
    std::cout << "  [B] FSM 상태 객체 new 후 미해제\n";
    std::cout << "  [C] 가상 소멸자 누락\n";
    std::cout << "  [D] 캐시 없이 반복 로딩\n";
    std::cout << "  [Q] 종료\n";
    std::cout << "----------------------------------------------------\n";

    while (true) {
        std::cout << "\n선택> ";
        char choice;
        std::cin >> choice;
        choice = toupper(choice);

        switch (choice) {
        case 'A': BugA_DestructorWithoutDelete(); break;
        case 'B': BugB_FSMStateLeaks(); break;
        case 'C': BugC_MissingVirtualDestructor(); break;
        case 'D': BugD_RepeatedLoadingWithoutCache(); break;
        case 'Q': std::cout << "종료합니다.\n"; return 0;
        default:  std::cout << "잘못된 입력입니다.\n"; break;
        }
    }
}
