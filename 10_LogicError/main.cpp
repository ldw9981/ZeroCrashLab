/*============================================================================
 *  ZeroCrashLab - 10. Logic Error (논리 오류)
 *  ---------------------------------------------------------------------------
 *  즉시 크래시하지 않지만 잘못된 동작을 유발하고,
 *  축적되면 크래시로 이어질 수 있는 유형입니다.
 *
 *  [교육 목표] 논리적 오류를 찾고 올바르게 수정하세요.
 *  이 프로그램은 크래시 대신 잘못된 동작 결과를 보여줍니다.
 *============================================================================*/
#include <iostream>
#include <string>
#include <vector>
#include <map>

// ============================================================================
// BUG A: 값 복사 vs 참조 혼동
// ============================================================================
void BugA_CopyVsReference() {
    std::cout << "\n[BUG A] 값 복사 vs 참조 혼동\n";
    std::cout << "  auto로 값 복사를 받으면 원본은 변경되지 않습니다.\n\n";

    std::map<std::string, std::vector<int>> objectMap;
    objectMap["enemies"] = {1, 2, 3, 4, 5, 6, 7, 8};

    std::cout << "  삭제 전 enemies: ";
    for (int id : objectMap["enemies"]) std::cout << id << " ";
    std::cout << "(크기: " << objectMap["enemies"].size() << ")\n";

    // BAD: auto (참조 아님!)로 값 복사 → 복사본만 수정됨
    auto container = objectMap["enemies"];  // 값 복사!
    for (auto it = container.begin(); it != container.end(); ) {
        if (*it % 2 == 0) {
            it = container.erase(it);  // 복사본만 수정!
        } else {
            ++it;
        }
    }

    std::cout << "  삭제 후 enemies: ";
    for (int id : objectMap["enemies"]) std::cout << id << " ";
    std::cout << "(크기: " << objectMap["enemies"].size() << ")\n";

    std::cout << "  [결과] 짝수를 삭제했다고 생각했지만 원본은 그대로!\n";
    std::cout << "  → 삭제된 오브젝트가 맵에 영구 잔존 → 댕글링 참조 위험!\n";
}

// ============================================================================
// BUG B: if / else if 누락 (중복 풀 추가)
// ============================================================================
struct Enemy {
    std::string type;
    Enemy(const std::string& t) : type(t) {}
};

void BugB_MissingElseIf() {
    std::cout << "\n[BUG B] if / else if 누락 (중복 풀 추가)\n";
    std::cout << "  else if가 아닌 독립 if를 쓰면 중복 분기가 됩니다.\n\n";

    // 한 적이 두 가지 타입을 동시에 가진 경우
    struct MultiTypeEnemy {
        bool isNormal = true;
        bool isArcher = true;  // 둘 다 true!
        bool isThief = false;
    };

    MultiTypeEnemy enemy;
    std::vector<std::string> normalPool, archerPool, thiefPool;

    // BAD: else if가 아닌 if → 모든 조건이 독립적으로 평가됨!
    if (enemy.isNormal)
        normalPool.push_back("Enemy_1");
    if (enemy.isArcher)    // else if가 아님!
        archerPool.push_back("Enemy_1");
    if (enemy.isThief)     // else if가 아님!
        thiefPool.push_back("Enemy_1");

    std::cout << "  normalPool 크기: " << normalPool.size() << "\n";
    std::cout << "  archerPool 크기: " << archerPool.size() << "\n";
    std::cout << "  thiefPool  크기: " << thiefPool.size() << "\n";
    std::cout << "  [결과] 같은 적이 2개 풀에 동시에 추가됨 → 중복 재사용 → 데이터 손상!\n";
}

// ============================================================================
// BUG C: Dangling Else (중괄호 누락)
// ============================================================================
void BugC_DanglingElse() {
    std::cout << "\n[BUG C] Dangling Else (중괄호 누락)\n";
    std::cout << "  중괄호가 없으면 else가 의도한 if에 바인딩되지 않습니다.\n\n";

    bool isAlive = true;
    bool hasWeapon = false;

    std::cout << "  isAlive = true, hasWeapon = false\n";
    std::cout << "  의도: isAlive가 false일 때 \"Dead\" 출력\n";
    std::cout << "  실제 동작:\n";

    // BAD: else가 내부 if(hasWeapon)에 바인딩됨!
    if (isAlive)
        if (hasWeapon)
            std::cout << "    -> Armed and alive!\n";
    else                                                    // 들여쓰기와 다르게
        std::cout << "    -> Dead! (의도는 isAlive==false)\n"; // 실제로는 isAlive && !hasWeapon

    std::cout << "\n  [결과] 살아있는데 무기가 없으면 \"Dead\"가 출력됩니다!\n";
    std::cout << "  들여쓰기와 실제 동작이 다릅니다 (dangling else).\n";
}

// ============================================================================
// BUG D: 변수명과 실제 의미 불일치
// ============================================================================
void BugD_MisleadingVariableName() {
    std::cout << "\n[BUG D] 변수명과 실제 의미 불일치\n";
    std::cout << "  변수명이 반대 의미를 나타내면 로직이 뒤집힙니다.\n\n";

    std::vector<std::string> cameras;  // 현재 카메라 없음

    // BAD: hasCamera인데 empty()의 결과를 저장!
    // hasCamera == true → 실제로는 카메라가 "없다"는 의미!
    bool hasCamera = cameras.empty();

    std::cout << "  cameras.size() = " << cameras.size() << "\n";
    std::cout << "  hasCamera = " << hasCamera << " (true면 카메라가 있다는 뜻?)\n";

    // hasCamera를 믿고 카메라를 추가하지 않음
    if (hasCamera) {
        std::cout << "  -> 카메라가 이미 있으므로 추가하지 않음\n";
    } else {
        std::cout << "  -> 카메라가 없으므로 기본 카메라 추가\n";
    }

    std::cout << "\n  [결과] 카메라가 0개인데 hasCamera=true → 카메라를 추가하지 않음!\n";
    std::cout << "  → 게임 시작 시 활성 카메라가 없어서 검은 화면!\n";
}

// ============================================================================
// BUG E: 잘못된 변수 전달
// ============================================================================
void BugE_WrongVariablePassed() {
    std::cout << "\n[BUG E] 잘못된 변수 전달\n";
    std::cout << "  계산한 변수 대신 원본 변수를 전달하는 실수.\n\n";

    struct Vector3 { float x, y, z; };

    Vector3 buildingPos = {10.0f, 0.0f, 10.0f};  // 건물 위치
    Vector3 bulletPos = buildingPos;
    bulletPos.y = 5.0f;  // 총알은 높이 5에서 발사해야 함

    std::cout << "  건물 위치:     (" << buildingPos.x << ", " << buildingPos.y
              << ", " << buildingPos.z << ")\n";
    std::cout << "  총알 시작위치: (" << bulletPos.x << ", " << bulletPos.y
              << ", " << bulletPos.z << ")\n";

    // BAD: bulletPos 대신 buildingPos를 전달!
    auto FireBullet = [](Vector3 startPos, float speed) {
        std::cout << "  -> 총알 발사 위치: (" << startPos.x << ", "
                  << startPos.y << ", " << startPos.z << ")\n";
    };

    FireBullet(buildingPos, 10.0f);  // bulletPos가 아닌 buildingPos!

    std::cout << "\n  [결과] 총알이 y=0(지면)에서 발사됩니다 (y=5가 아닌)!\n";
}

// ============================================================================
// BUG F: 미사용 기능 (데드 코드)
// ============================================================================
class DebuffBuilding {
    std::string name;
    int debuffPower;
public:
    DebuffBuilding(const std::string& n, int p) : name(n), debuffPower(p) {}

    void Update() {
        // BAD: GiveDebuff()를 호출하지 않음!
        // 디버프 건물이 실제로 아무 기능도 하지 않음
        std::cout << "    " << name << " Update() 실행됨\n";
    }

    void GiveDebuff() {
        // 이 함수는 구현되어 있지만 어디서도 호출되지 않음!
        std::cout << "    " << name << " 디버프 적용! (파워: " << debuffPower << ")\n";
    }
};

void BugF_DeadCode() {
    std::cout << "\n[BUG F] 미사용 기능 (데드 코드)\n";
    std::cout << "  구현했지만 호출하지 않는 함수.\n\n";

    DebuffBuilding tower("Frost Tower", 30);
    std::cout << "  5프레임 동안 Update 실행:\n";
    for (int i = 0; i < 5; i++) {
        tower.Update();  // GiveDebuff()가 호출되지 않음!
    }
    std::cout << "\n  [결과] 디버프 건물이 아무 효과도 주지 않습니다!\n";
    std::cout << "  GiveDebuff()가 구현되었지만 Update()에서 호출하지 않음.\n";
}

// ============================================================================
// 메인
// ============================================================================
int main() {
    std::cout << "====================================================\n";
    std::cout << "  ZeroCrashLab - 10. Logic Error\n";
    std::cout << "  (논리 오류)\n";
    std::cout << "====================================================\n";
    std::cout << "\n  이 프로그램은 논리적 오류를 시연합니다.\n";
    std::cout << "  크래시 대신 잘못된 동작을 관찰하세요.\n";
    std::cout << "  교육생은 논리 오류를 찾고 수정하세요.\n\n";
    std::cout << "  [A] 값 복사 vs 참조 혼동\n";
    std::cout << "  [B] if / else if 누락 (중복 풀 추가)\n";
    std::cout << "  [C] Dangling Else (중괄호 누락)\n";
    std::cout << "  [D] 변수명과 실제 의미 불일치\n";
    std::cout << "  [E] 잘못된 변수 전달\n";
    std::cout << "  [F] 미사용 기능 (데드 코드)\n";
    std::cout << "  [Q] 종료\n";
    std::cout << "----------------------------------------------------\n";

    while (true) {
        std::cout << "\n선택> ";
        char choice;
        std::cin >> choice;
        choice = toupper(choice);

        switch (choice) {
        case 'A': BugA_CopyVsReference(); break;
        case 'B': BugB_MissingElseIf(); break;
        case 'C': BugC_DanglingElse(); break;
        case 'D': BugD_MisleadingVariableName(); break;
        case 'E': BugE_WrongVariablePassed(); break;
        case 'F': BugF_DeadCode(); break;
        case 'Q': std::cout << "종료합니다.\n"; return 0;
        default:  std::cout << "잘못된 입력입니다.\n"; break;
        }
    }
}
