/*============================================================================
 *  ZeroCrashLab - 06. Uninitialized Variable (미초기화 변수)
 *  ---------------------------------------------------------------------------
 *  C++에서 지역 변수와 POD 멤버 변수는 자동으로 초기화되지 않습니다.
 *  미초기화 포인터는 가비지 주소를 가리켜 역참조 시 크래시하고,
 *  미초기화 숫자/bool은 잘못된 계산이나 분기를 유발합니다.
 *
 *  [교육 목표] 초기화되지 않은 변수가 어떤 문제를 일으키는지 확인하고 수정하세요.
 *  힌트: 모든 멤버 변수를 선언 시점에 초기화하세요 (= nullptr, = 0, = false, = {})
 *============================================================================*/
#include <iostream>
#include <string>
#include <cstring>

// ============================================================================
// BUG A: 미초기화 포인터 멤버
// ============================================================================
class Component {
public:
    virtual ~Component() = default;
    virtual std::string GetName() const = 0;
};

class Transform : public Component {
public:
    float x, y, z;
    std::string GetName() const override { return "Transform"; }
};

class Editor {
public:
    // BAD: nullptr로 초기화하지 않음!
    // 가비지 주소를 가리키므로 if(selectedObject) 가 true가 됨
    Component* selectedObject;

    void DrawInspector() {
        // 가비지 포인터는 0이 아니므로 이 조건이 true!
        if (selectedObject) {
            std::cout << "    선택된 오브젝트: " << selectedObject->GetName() << "\n";
            // 가비지 주소 역참조 → 크래시!
        } else {
            std::cout << "    선택된 오브젝트 없음\n";
        }
    }
};

void BugA_UninitializedPointer() {
    std::cout << "\n[BUG A] 미초기화 포인터 멤버\n";
    std::cout << "  포인터를 초기화하지 않으면 가비지 주소를 가리킵니다.\n";
    std::cout << "  if(ptr)로 검사해도 가비지 주소는 0이 아니므로 통과!\n\n";

    Editor editor;  // selectedObject가 초기화되지 않음!
    std::cout << "  selectedObject 주소: " << editor.selectedObject
              << " (가비지 - nullptr이 아님!)\n";
    std::cout << "  DrawInspector() 호출...\n";
    editor.DrawInspector();  // CRASH!
}

// ============================================================================
// BUG B: 미초기화 bool/숫자 멤버
// ============================================================================
class PlayerController {
public:
    // BAD: bool 멤버가 초기화되지 않음!
    bool isInputMoveForward;
    bool isInputMoveBackward;
    bool isInputRun;
    bool isJumping;
    float moveSpeed;
    int hp;

    void PrintState() {
        // 가비지 값이므로 예측 불가능한 결과
        std::cout << "    isInputMoveForward  = " << isInputMoveForward << "\n";
        std::cout << "    isInputMoveBackward = " << isInputMoveBackward << "\n";
        std::cout << "    isInputRun          = " << isInputRun << "\n";
        std::cout << "    isJumping           = " << isJumping << "\n";
        std::cout << "    moveSpeed           = " << moveSpeed << "\n";
        std::cout << "    hp                  = " << hp << "\n";
    }

    void Update() {
        // 가비지 bool이 true이면 의도하지 않은 동작!
        if (isInputMoveForward) {
            std::cout << "    [!] 플레이어가 입력 없이 앞으로 이동합니다!\n";
        }
        if (isInputRun) {
            std::cout << "    [!] 플레이어가 입력 없이 달립니다!\n";
        }
        if (isJumping) {
            std::cout << "    [!] 플레이어가 입력 없이 점프합니다!\n";
        }
    }
};

void BugB_UninitializedBoolAndNumbers() {
    std::cout << "\n[BUG B] 미초기화 bool/숫자 멤버\n";
    std::cout << "  bool/int/float을 초기화하지 않으면 가비지 값이 됩니다.\n\n";

    PlayerController pc;  // 모든 멤버가 가비지!
    std::cout << "  PlayerController 멤버 (가비지 값):\n";
    pc.PrintState();
    std::cout << "\n  Update() 호출 (가비지 bool로 분기):\n";
    pc.Update();
    std::cout << "\n  [결과] 초기 프레임에서 예기치 않은 이동/점프가 발생합니다!\n";
}

// ============================================================================
// BUG C: 미초기화 구조체 배열
// ============================================================================
struct RenderItem {
    // BAD: 모든 멤버가 미초기화!
    void* vertexBuffer;
    void* indexBuffer;
    unsigned int indexCount;
    unsigned int boneCount;
    float* boneMatrices;
};

void BugC_UninitializedStruct() {
    std::cout << "\n[BUG C] 미초기화 구조체 배열\n";
    std::cout << "  구조체의 POD 멤버는 자동 초기화되지 않습니다.\n\n";

    RenderItem items[3];  // 3개 모두 가비지!

    for (int i = 0; i < 3; i++) {
        std::cout << "  items[" << i << "]:\n";
        std::cout << "    vertexBuffer = " << items[i].vertexBuffer << "\n";
        std::cout << "    indexBuffer  = " << items[i].indexBuffer << "\n";
        std::cout << "    indexCount   = " << items[i].indexCount << "\n";
        std::cout << "    boneCount    = " << items[i].boneCount << "\n";
        std::cout << "    boneMatrices = " << items[i].boneMatrices << "\n";
    }

    std::cout << "\n  렌더링 시도 (가비지 포인터 역참조)...\n";
    // BAD: 가비지 indexCount만큼 렌더링 시도하면 엄청난 반복 또는 크래시!
    if (items[0].indexCount > 0 && items[0].indexCount < 100) {
        std::cout << "    가비지 indexCount(" << items[0].indexCount << ")만큼 Draw 시도!\n";
    } else {
        std::cout << "    가비지 indexCount = " << items[0].indexCount
                  << " (터무니없는 값!)\n";
    }
    std::cout << "  [결과] GPU가 가비지 데이터로 렌더링 → 화면 깨짐 또는 크래시!\n";
}

// ============================================================================
// BUG D: Handle/Slot 미초기화 (유효성 검사 오동작)
// ============================================================================
struct Handle {
    unsigned int index;       // BAD: 가비지!
    unsigned int generation;  // BAD: 가비지!
};

struct Slot {
    void* ptr;                // BAD: 가비지!
    unsigned int generation;  // BAD: 가비지!
};

bool IsHandleValid(const Handle& h, const Slot* slots, size_t slotCount) {
    if (h.index >= slotCount) return false;
    return slots[h.index].generation == h.generation && slots[h.index].ptr != nullptr;
}

void BugD_UninitializedHandle() {
    std::cout << "\n[BUG D] Handle/Slot 미초기화 (유효성 검사 오동작)\n";
    std::cout << "  Handle과 Slot의 generation이 가비지이면 유효성 검사가 오동작합니다.\n\n";

    Slot slots[4];  // 모든 generation이 가비지!
    Handle handle;  // index, generation 모두 가비지!

    std::cout << "  Handle: index=" << handle.index
              << ", generation=" << handle.generation << "\n";

    for (int i = 0; i < 4; i++) {
        std::cout << "  Slot[" << i << "]: ptr=" << slots[i].ptr
                  << ", generation=" << slots[i].generation << "\n";
    }

    bool valid = IsHandleValid(handle, slots, 4);
    std::cout << "\n  IsHandleValid = " << valid << "\n";
    std::cout << "  [결과] 가비지 generation이 우연히 일치하면 잘못된 객체에 접근!\n";
}

// ============================================================================
// 메인
// ============================================================================
int main() {
    std::cout << "====================================================\n";
    std::cout << "  ZeroCrashLab - 06. Uninitialized Variable\n";
    std::cout << "  (미초기화 변수)\n";
    std::cout << "====================================================\n";
    std::cout << "\n  이 프로그램은 미초기화 변수의 위험성을 시연합니다.\n";
    std::cout << "  교육생은 초기화 누락을 찾고 수정하세요.\n\n";
    std::cout << "  [A] 미초기화 포인터 멤버 (크래시)\n";
    std::cout << "  [B] 미초기화 bool/숫자 멤버 (오동작)\n";
    std::cout << "  [C] 미초기화 구조체 배열 (가비지 렌더링)\n";
    std::cout << "  [D] Handle/Slot 미초기화 (유효성 검사 오동작)\n";
    std::cout << "  [Q] 종료\n";
    std::cout << "----------------------------------------------------\n";

    while (true) {
        std::cout << "\n선택> ";
        char choice;
        std::cin >> choice;
        choice = toupper(choice);

        switch (choice) {
        case 'A': BugA_UninitializedPointer(); break;
        case 'B': BugB_UninitializedBoolAndNumbers(); break;
        case 'C': BugC_UninitializedStruct(); break;
        case 'D': BugD_UninitializedHandle(); break;
        case 'Q': std::cout << "종료합니다.\n"; return 0;
        default:  std::cout << "잘못된 입력입니다.\n"; break;
        }
    }
}
