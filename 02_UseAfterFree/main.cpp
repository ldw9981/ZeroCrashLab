/*============================================================================
 *  ZeroCrashLab - 02. Use-After-Free / Dangling Pointer
 *  ---------------------------------------------------------------------------
 *  메모리가 해제된 후에도 포인터가 여전히 그 주소를 가리키고 있어
 *  접근 시 크래시하는 유형입니다. 힙 손상, 크래시, 데이터 오염 등
 *  가장 디버깅이 어려운 버그 유형입니다.
 *
 *  [교육 목표] 아래 코드에서 해제된 메모리에 접근하는 부분을 찾고 수정하세요.
 *============================================================================*/
#include <iostream>
#include <string>
#include <vector>
#include <memory>

// ============================================================================
// 간이 클래스들
// ============================================================================

class GameObject {
    std::string name;
    int hp;
public:
    GameObject(const std::string& n, int h) : name(n), hp(h) {}
    const std::string& GetName() const { return name; }
    int GetHP() const { return hp; }
    void TakeDamage(int d) { hp -= d; std::cout << "    " << name << " took " << d << " damage. HP: " << hp << "\n"; }
};

// ============================================================================
// BUG A: SAFE_DELETE가 실제로 동작하지 않음
// - 포인터를 "값"으로 전달하므로 호출자의 포인터가 null이 되지 않음
// - 두 번째 호출 시 double-free 크래시
// ============================================================================

// BAD: 포인터를 값(value)으로 받으므로 호출자의 포인터는 변경되지 않음!
template <typename T>
void SAFE_DELETE(T* p) {
    if (p) {
        delete p;
        p = nullptr;  // 로컬 복사본만 nullptr이 됨!
    }
}

void BugA_SafeDeleteByValue() {
    std::cout << "\n[BUG A] SAFE_DELETE 값 전달 (double-free)\n";
    std::cout << "  포인터를 값으로 전달하면 원본 포인터는 null이 되지 않습니다...\n";

    GameObject* player = new GameObject("Player", 100);
    std::cout << "  생성: " << player->GetName() << "\n";

    SAFE_DELETE(player);  // 메모리는 해제되지만 player는 여전히 원래 주소!
    std::cout << "  SAFE_DELETE 후 player 포인터: " << player << " (null이 아님!)\n";

    // BAD: player는 이미 해제된 메모리를 가리킴 → double-free!
    SAFE_DELETE(player);  // CRASH! double-free
    std::cout << "  이 메시지는 보이지 않을 것입니다.\n";
}

// ============================================================================
// BUG B: 오브젝트 풀 반환 후 참조 유지
// - 삭제된 객체의 포인터를 계속 사용
// ============================================================================
void BugB_UseAfterDelete() {
    std::cout << "\n[BUG B] delete 후 포인터 사용\n";
    std::cout << "  삭제된 객체의 포인터로 계속 접근합니다...\n";

    GameObject* enemy = new GameObject("Goblin", 50);
    GameObject* cachedTarget = enemy;  // 다른 곳에서 참조를 캐시

    std::cout << "  적 생성: " << enemy->GetName() << " (HP: " << enemy->GetHP() << ")\n";
    std::cout << "  cachedTarget에 포인터 캐시\n";

    // 적이 죽어서 삭제
    delete enemy;
    enemy = nullptr;
    std::cout << "  적 삭제 완료. enemy = nullptr\n";

    // BAD: cachedTarget은 여전히 해제된 메모리를 가리킴!
    std::cout << "  cachedTarget으로 접근 시도...\n";
    cachedTarget->TakeDamage(10);  // CRASH! Use-After-Free
}

// ============================================================================
// BUG C: 지역 변수 참조 반환
// - 함수가 끝나면 지역 변수가 소멸되는데 그 참조를 반환
// ============================================================================
const std::string& GetEnemyName(int id) {
    if (id == 1) {
        static const std::string name1 = "Goblin";
        return name1;
    }
    // BAD: 임시 string을 참조로 반환 → 함수 종료 후 댕글링 참조!
    return std::string("Unknown_" + std::to_string(id));
}

void BugC_DanglingReference() {
    std::cout << "\n[BUG C] 지역 변수 참조 반환 (Dangling Reference)\n";
    std::cout << "  임시 객체의 참조를 반환하면 댕글링 참조가 됩니다...\n";

    const std::string& name1 = GetEnemyName(1);
    std::cout << "  ID 1: " << name1 << " (static이므로 안전)\n";

    // BAD: id=2일 때 임시 string의 참조가 반환됨 → 이미 소멸된 객체!
    const std::string& name2 = GetEnemyName(2);
    std::cout << "  ID 2: " << name2 << "\n";  // UB! 가비지 또는 크래시
}

// ============================================================================
// BUG D: vector 재할당 후 댕글링 포인터
// - vector가 push_back으로 재할당되면 기존 포인터/참조가 무효화
// ============================================================================
void BugD_VectorReallocation() {
    std::cout << "\n[BUG D] vector 재할당 후 댕글링 포인터\n";
    std::cout << "  push_back으로 벡터가 재할당되면 기존 포인터가 무효화됩니다...\n";

    std::vector<GameObject> enemies;
    enemies.reserve(2);  // 딱 2개만 예약

    enemies.emplace_back("Goblin", 30);
    enemies.emplace_back("Skeleton", 50);

    // 첫 번째 적의 포인터를 캐시
    GameObject* firstEnemy = &enemies[0];
    std::cout << "  첫 번째 적: " << firstEnemy->GetName() << "\n";

    // push_back으로 capacity 초과 → 벡터 전체 재할당!
    std::cout << "  push_back 3번째 → capacity 초과 → 재할당 발생!\n";
    enemies.emplace_back("Dragon", 200);

    // BAD: firstEnemy는 이전 메모리 블록을 가리킴 → 댕글링!
    std::cout << "  캐시된 포인터로 접근: " << firstEnemy->GetName() << "\n";  // UB!
    firstEnemy->TakeDamage(5);  // CRASH 가능!
}

// ============================================================================
// 메인
// ============================================================================
int main() {
    std::cout << "====================================================\n";
    std::cout << "  ZeroCrashLab - 02. Use-After-Free / Dangling Pointer\n";
    std::cout << "====================================================\n";
    std::cout << "\n  이 프로그램은 의도적으로 크래시를 발생시킵니다.\n";
    std::cout << "  교육생은 크래시 원인을 파악하고 코드를 수정하세요.\n\n";
    std::cout << "  [A] SAFE_DELETE 값 전달 (double-free)\n";
    std::cout << "  [B] delete 후 포인터 사용\n";
    std::cout << "  [C] 지역 변수 참조 반환 (Dangling Reference)\n";
    std::cout << "  [D] vector 재할당 후 댕글링 포인터\n";
    std::cout << "  [Q] 종료\n";
    std::cout << "----------------------------------------------------\n";

    while (true) {
        std::cout << "\n선택> ";
        char choice;
        std::cin >> choice;
        choice = toupper(choice);

        switch (choice) {
        case 'A': BugA_SafeDeleteByValue(); break;
        case 'B': BugB_UseAfterDelete(); break;
        case 'C': BugC_DanglingReference(); break;
        case 'D': BugD_VectorReallocation(); break;
        case 'Q': std::cout << "종료합니다.\n"; return 0;
        default:  std::cout << "잘못된 입력입니다.\n"; break;
        }
    }
}
