/*============================================================================
 *  ZeroCrashLab - 09. Missing Return Value / Undefined Behavior
 *  ---------------------------------------------------------------------------
 *  함수가 값을 반환해야 하는데 특정 경로에서 return이 없거나,
 *  C++ 표준이 보장하지 않는 동작에 의존하는 유형입니다.
 *
 *  [교육 목표] UB를 유발하는 패턴을 찾고 수정하세요.
 *============================================================================*/
#include <iostream>
#include <string>
#include <vector>
#include <map>

// ============================================================================
// BUG A: 함수의 모든 경로에 return이 없음
// ============================================================================
class GameObject {
    std::string name;
public:
    GameObject(const std::string& n) : name(n) {}
    const std::string& GetName() const { return name; }
};

#pragma warning(push)
#pragma warning(disable: 4715)  // 교육용: "not all control paths return a value" 경고 억제

// BAD: 루프에서 조건을 만족하지 못하면 return이 없음!
GameObject* FindRootObject(std::vector<GameObject*>& objects, std::vector<int>& parentIDs) {
    for (int i = 0; i < (int)objects.size(); ++i) {
        if (objects[i] && parentIDs[i] == -1) {
            return objects[i];  // 루트 오브젝트 반환
        }
    }
    // 여기에 도달하면 return이 없음! → UB → 가비지 포인터 반환!
}

// BAD: map에서 못 찾으면 return 없음!
GameObject* GetGameObject(std::map<std::string, GameObject*>& objects, const std::string& name) {
    auto it = objects.find(name);
    if (it != objects.end()) {
        return it->second;
    }
    // return 없음! → UB!
}

#pragma warning(pop)

void BugA_MissingReturn() {
    std::cout << "\n[BUG A] 함수의 모든 경로에 return이 없음\n";
    std::cout << "  non-void 함수에서 return 없이 끝나면 가비지 반환 → 크래시!\n\n";

    // Case 1: 모든 parentID가 -1이 아닌 경우
    std::vector<GameObject*> objects = { new GameObject("Child_A"), new GameObject("Child_B") };
    std::vector<int> parentIDs = { 0, 1 };  // 루트 없음!

    std::cout << "  FindRootObject() 호출 (루트 없는 경우)...\n";
    GameObject* root = FindRootObject(objects, parentIDs);
    // root는 가비지 포인터!
    std::cout << "  root = " << root << "\n";
    std::cout << "  root->GetName() 호출...\n";
    std::cout << "  " << root->GetName() << "\n";  // CRASH! 가비지 포인터 역참조

    for (auto* o : objects) delete o;
}

void BugA2_MissingReturn_Map() {
    std::cout << "\n[BUG A-2] 맵에서 못 찾을 때 return 없음\n\n";

    std::map<std::string, GameObject*> objects;
    objects["Hero"] = new GameObject("Hero");

    std::cout << "  GetGameObject(\"Villain\") 호출 (없는 키)...\n";
    GameObject* obj = GetGameObject(objects, "Villain");
    std::cout << "  obj = " << obj << "\n";
    std::cout << "  obj->GetName() 호출...\n";
    std::cout << "  " << obj->GetName() << "\n";  // CRASH!

    for (auto& [k, v] : objects) delete v;
}

// ============================================================================
// BUG B: == 를 = 대신 사용 (no-op)
// ============================================================================
void BugB_ComparisonInsteadOfAssignment() {
    std::cout << "\n[BUG B] == 를 = 대신 사용 (no-op 오타)\n";
    std::cout << "  비교 연산의 결과를 버리면 아무 효과가 없습니다.\n\n";

    float attackTimer = 5.0f;
    std::cout << "  공격 타이머 리셋 전: " << attackTimer << "\n";

    // BAD: = 대신 == 사용! 비교 결과(true/false)가 버려짐!
    attackTimer == 0.0f;  // 아무 효과 없음!

    std::cout << "  공격 타이머 리셋 후: " << attackTimer << " (변경 안 됨!)\n";
    std::cout << "  [결과] 타이머가 리셋되지 않아 즉시 재공격 가능!\n";
}

// ============================================================================
// BUG C: 역참조 후 null 검사 (순서 오류)
// ============================================================================
struct PhysicsHit {
    std::string* actorName;
};

void BugC_DereferenceBeforeNullCheck() {
    std::cout << "\n[BUG C] 역참조 후 null 검사 (순서 오류)\n";
    std::cout << "  먼저 역참조하고 나중에 null 검사하면 이미 크래시!\n\n";

    PhysicsHit hit;
    hit.actorName = nullptr;  // actor가 없는 경우

    // BAD: 역참조를 먼저 하고 null 검사를 나중에!
    std::cout << "  hit.actorName 역참조 시도...\n";
    std::string name = *hit.actorName;  // CRASH! nullptr 역참조!

    if (hit.actorName == nullptr) {  // 데드 코드! 위에서 이미 크래시
        std::cout << "  actor 없음\n";
        return;
    }
    std::cout << "  actor: " << name << "\n";
}

// ============================================================================
// BUG D: 무한 재귀 (스택 오버플로)
// ============================================================================
struct TreeNode {
    std::string name;
    std::vector<TreeNode*> children;
    TreeNode* parent = nullptr;

    TreeNode(const std::string& n) : name(n) {}
    ~TreeNode() { for (auto* c : children) delete c; }
};

void MarkDirty(TreeNode* node) {
    if (!node) return;
    std::cout << "    Dirty: " << node->name << "\n";
    for (auto* child : node->children) {
        MarkDirty(child);  // 순환 참조가 있으면 무한 재귀!
    }
}

void BugD_InfiniteRecursion() {
    std::cout << "\n[BUG D] 무한 재귀 (스택 오버플로)\n";
    std::cout << "  순환 참조가 있는 트리에서 재귀 순회하면 스택 오버플로!\n\n";

    TreeNode* root = new TreeNode("Root");
    TreeNode* childA = new TreeNode("ChildA");
    TreeNode* childB = new TreeNode("ChildB");

    root->children.push_back(childA);
    childA->children.push_back(childB);
    // BAD: 순환 참조 생성!
    childB->children.push_back(root);

    std::cout << "  트리 구조: Root -> ChildA -> ChildB -> Root (순환!)\n";
    std::cout << "  MarkDirty(root) 호출...\n";

    MarkDirty(root);  // 무한 재귀 → 스택 오버플로!

    // 메모리 정리 (순환 참조 때문에 delete 위험하므로 수동 정리)
    childB->children.clear();  // 순환 참조 끊기
    delete root;
}

// ============================================================================
// 메인
// ============================================================================
int main() {
    std::cout << "====================================================\n";
    std::cout << "  ZeroCrashLab - 09. Undefined Behavior\n";
    std::cout << "  (반환값 누락 / 정의되지 않은 동작)\n";
    std::cout << "====================================================\n";
    std::cout << "\n  이 프로그램은 UB 상황을 시연합니다.\n";
    std::cout << "  교육생은 UB 원인을 파악하고 수정하세요.\n\n";
    std::cout << "  [A] 함수 반환값 누락 (FindRoot)\n";
    std::cout << "  [B] 함수 반환값 누락 (Map 조회)\n";
    std::cout << "  [C] == vs = 오타 (no-op)\n";
    std::cout << "  [D] 역참조 후 null 검사 (순서 오류)\n";
    std::cout << "  [E] 무한 재귀 (스택 오버플로)\n";
    std::cout << "  [Q] 종료\n";
    std::cout << "----------------------------------------------------\n";

    while (true) {
        std::cout << "\n선택> ";
        char choice;
        std::cin >> choice;
        choice = toupper(choice);

        switch (choice) {
        case 'A': BugA_MissingReturn(); break;
        case 'B': BugA2_MissingReturn_Map(); break;
        case 'C': BugB_ComparisonInsteadOfAssignment(); break;
        case 'D': BugC_DereferenceBeforeNullCheck(); break;
        case 'E': BugD_InfiniteRecursion(); break;
        case 'Q': std::cout << "종료합니다.\n"; return 0;
        default:  std::cout << "잘못된 입력입니다.\n"; break;
        }
    }
}
