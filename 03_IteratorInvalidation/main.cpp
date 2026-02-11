/*============================================================================
 *  ZeroCrashLab - 03. Iterator Invalidation (반복자 무효화)
 *  ---------------------------------------------------------------------------
 *  컬렉션을 순회하는 도중 컬렉션 자체를 수정하여 반복자가 무효화되는 유형.
 *  vector의 push_back, erase, map의 insert 등이 순회 중 호출되면
 *  100% Undefined Behavior로 크래시 또는 데이터 손상이 발생합니다.
 *
 *  [교육 목표] 순회 중 컬렉션 수정이 왜 위험한지 이해하고 수정하세요.
 *============================================================================*/
#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>

// ============================================================================
// BUG A: range-for 내부에서 push_back
// - 벡터 재할당 발생 → 외부 루프의 반복자가 무효화
// - TigerEngine S-1에서 발견된 가장 위험한 패턴
// ============================================================================
void BugA_PushBackDuringIteration() {
    std::cout << "\n[BUG A] range-for 내부에서 push_back\n";
    std::cout << "  순회 중 벡터에 원소를 추가하면 재할당으로 반복자가 무효화됩니다...\n";

    std::vector<int> scripts = {1, 2, 3};
    std::vector<int> pendingScripts = {10, 20, 30, 40, 50};

    std::cout << "  순회 시작 (scripts 크기: " << scripts.size() << ")\n";

    // BAD: scripts를 순회하면서 scripts에 push_back!
    for (auto& s : scripts) {
        std::cout << "    처리 중: " << s << "\n";
        for (auto& p : pendingScripts) {
            scripts.push_back(p);  // 재할당 발생! 외부 반복자 무효화!
        }
        pendingScripts.clear();
        // 여기서 s는 이미 댕글링 참조 → 크래시!
    }
    std::cout << "  이 메시지는 보이지 않을 것입니다.\n";
}

// ============================================================================
// BUG B: range-for 내부에서 erase
// - 순회 중 원소를 제거하면 반복자가 무효화
// ============================================================================
void BugB_EraseDuringIteration() {
    std::cout << "\n[BUG B] range-for 내부에서 erase\n";
    std::cout << "  순회 중 원소를 제거하면 반복자가 무효화됩니다...\n";

    std::vector<std::string> cameras = {"Main", "UI", "Debug", "Cinematic", "Minimap"};
    std::cout << "  카메라 목록: ";
    for (auto& c : cameras) std::cout << c << " ";
    std::cout << "\n";

    // BAD: range-for로 순회하면서 erase!
    for (auto& cam : cameras) {
        std::cout << "    검사 중: " << cam << "\n";
        if (cam == "Debug" || cam == "Minimap") {
            // erase를 위해 반복자를 찾는다
            auto it = std::find(cameras.begin(), cameras.end(), cam);
            cameras.erase(it);  // 반복자 무효화! → 크래시 또는 원소 건너뛰기
        }
    }
}

// ============================================================================
// BUG C: unordered_map 순회 중 삽입
// - insert로 rehash가 발생하면 모든 반복자가 무효화
// ============================================================================
void BugC_MapInsertDuringIteration() {
    std::cout << "\n[BUG C] unordered_map 순회 중 삽입\n";
    std::cout << "  순회 중 맵에 새 항목을 삽입하면 rehash로 반복자가 무효화됩니다...\n";

    std::unordered_map<int, std::string> entities;
    entities[1] = "Player";
    entities[2] = "Enemy_A";
    entities[3] = "Enemy_B";

    int nextId = 100;

    // BAD: 순회 중 새 엔티티 삽입 → rehash → 반복자 무효화!
    for (auto& [id, name] : entities) {
        std::cout << "    Update: [" << id << "] " << name << "\n";
        if (name.find("Enemy") != std::string::npos) {
            // 적이 새 적을 스폰
            entities[nextId] = "Spawned_" + std::to_string(nextId);
            std::cout << "    -> 스폰: [" << nextId << "]\n";
            nextId++;
        }
    }
    std::cout << "  이 메시지는 보이지 않을 것입니다.\n";
}

// ============================================================================
// BUG D: 맵 복사본 수정 (원본 미반영)
// - auto로 값 복사를 받아서 복사본만 수정, 원본은 그대로
// - 크래시는 아니지만 삭제가 동작하지 않는 심각한 로직 버그
// ============================================================================
void BugD_MapCopyModification() {
    std::cout << "\n[BUG D] 맵 복사본 수정 (원본 미반영)\n";
    std::cout << "  auto로 값 복사를 하면 원본 맵은 변경되지 않습니다...\n";

    std::unordered_map<std::string, std::vector<int>> objectMap;
    objectMap["enemies"] = {1, 2, 3, 4, 5};
    objectMap["items"] = {10, 20, 30};

    std::cout << "  삭제 전 enemies 크기: " << objectMap["enemies"].size() << "\n";

    // BAD: auto (참조 아님!)로 값 복사 → 복사본만 수정됨
    for (auto& [name, _] : objectMap) {
        auto container = objectMap[name];  // 값 복사!
        // 짝수 ID만 제거
        for (auto it = container.begin(); it != container.end(); ) {
            if (*it % 2 == 0) {
                it = container.erase(it);  // 복사본만 수정!
            } else {
                ++it;
            }
        }
    }

    // 원본은 변경되지 않음!
    std::cout << "  삭제 후 enemies 크기: " << objectMap["enemies"].size()
              << " (변경 안 됨!)\n";
    std::cout << "  [결과] 삭제했다고 생각했지만 원본은 그대로입니다.\n";
    std::cout << "  → 삭제된 오브젝트가 맵에 영구 잔존 → 댕글링 참조로 크래시!\n";
}

// ============================================================================
// 메인
// ============================================================================
int main() {
    std::cout << "====================================================\n";
    std::cout << "  ZeroCrashLab - 03. Iterator Invalidation\n";
    std::cout << "  (반복자 무효화)\n";
    std::cout << "====================================================\n";
    std::cout << "\n  이 프로그램은 의도적으로 크래시를 발생시킵니다.\n";
    std::cout << "  교육생은 크래시 원인을 파악하고 코드를 수정하세요.\n\n";
    std::cout << "  [A] range-for 내부에서 push_back\n";
    std::cout << "  [B] range-for 내부에서 erase\n";
    std::cout << "  [C] unordered_map 순회 중 삽입\n";
    std::cout << "  [D] 맵 복사본 수정 (원본 미반영 - 로직 버그)\n";
    std::cout << "  [Q] 종료\n";
    std::cout << "----------------------------------------------------\n";

    while (true) {
        std::cout << "\n선택> ";
        char choice;
        std::cin >> choice;
        choice = toupper(choice);

        switch (choice) {
        case 'A': BugA_PushBackDuringIteration(); break;
        case 'B': BugB_EraseDuringIteration(); break;
        case 'C': BugC_MapInsertDuringIteration(); break;
        case 'D': BugD_MapCopyModification(); break;
        case 'Q': std::cout << "종료합니다.\n"; return 0;
        default:  std::cout << "잘못된 입력입니다.\n"; break;
        }
    }
}
