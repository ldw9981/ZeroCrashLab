/*============================================================================
 *  ZeroCrashLab - 01. Null Pointer Dereference (널 포인터 역참조)
 *  ---------------------------------------------------------------------------
 *  전체 크래시 이슈의 약 40%를 차지하는 가장 빈번한 크래시 유형입니다.
 *  포인터가 nullptr인 상태에서 -> 또는 * 연산자로 접근하면
 *  Access Violation (0xC0000005) 으로 즉시 크래시합니다.
 *
 *  [교육 목표] 아래 코드에서 크래시가 발생하는 원인을 찾고 수정하세요.
 *  힌트: 포인터를 사용하기 전에 null인지 항상 확인해야 합니다.
 *============================================================================*/
#include <iostream>
#include <string>
#include <map>
#include <memory>

// ============================================================================
// 게임 엔진 시뮬레이션을 위한 간이 클래스들
// ============================================================================

class Component {
public:
    virtual ~Component() = default;
    virtual std::string GetName() const = 0;
};

class Transform : public Component {
    float x = 0, y = 0, z = 0;
public:
    std::string GetName() const override { return "Transform"; }
    float GetX() const { return x; }
};

class MeshRenderer : public Component {
public:
    std::string GetName() const override { return "MeshRenderer"; }
    void Render() { std::cout << "    [MeshRenderer] Rendering...\n"; }
};

class GameObject {
    std::string name;
    Transform* transform = nullptr;
    MeshRenderer* renderer = nullptr;
public:
    GameObject(const std::string& n) : name(n) {}
    ~GameObject() { delete transform; delete renderer; }

    void AddTransform() { transform = new Transform(); }
    void AddRenderer() { renderer = new MeshRenderer(); }

    const std::string& GetName() const { return name; }
    Transform* GetTransform() { return transform; }

    template<typename T>
    T* GetComponent();
};

template<> Transform* GameObject::GetComponent<Transform>() { return transform; }
template<> MeshRenderer* GameObject::GetComponent<MeshRenderer>() { return renderer; }

// 이름으로 오브젝트를 찾는 시스템 (nullptr 반환 가능!)
class SceneSystem {
    std::map<std::string, GameObject*> objects;
public:
    ~SceneSystem() {
        for (auto& [k, v] : objects) delete v;
    }
    void Register(const std::string& name, GameObject* obj) {
        objects[name] = obj;
    }
    GameObject* FindByName(const std::string& name) {
        auto it = objects.find(name);
        if (it != objects.end()) return it->second;
        return nullptr;
    }
};

// 싱글턴 패턴 시뮬레이션
class GameManager {
public:
    static GameManager* instance;  // 초기화되지 않을 수 있음!
    int score = 0;
    void AddScore(int s) { score += s; }
};
GameManager* GameManager::instance = nullptr;  // 아직 생성 안 됨!

// ============================================================================
// 크래시 시나리오들
// ============================================================================

/*
 * BUG A: Find/Get 체인에서 반환값 미검사
 * - FindByName()이 nullptr을 반환할 수 있는데 검사 없이 바로 역참조
 * - 실제 게임 엔진에서 가장 많이 발생하는 패턴 (TigerEngine: 45건)
 */
void BugA_FindGetChain(SceneSystem& scene) {
    std::cout << "\n[BUG A] Find/Get 체인 반환값 미검사\n";
    std::cout << "  존재하지 않는 오브젝트를 찾아서 바로 역참조합니다...\n";

    // BAD: "Player"라는 오브젝트가 없으면 nullptr이 반환되는데 바로 역참조!
    GameObject* player = scene.FindByName("Player");
    std::cout << "  플레이어 이름: " << player->GetName() << "\n";  // CRASH!
}

/*
 * BUG B: 싱글턴 인스턴스 미검사
 * - GameManager::instance가 nullptr인데 바로 접근
 * - 실제 게임에서 싱글턴 초기화 전에 다른 시스템이 접근하는 경우
 */
void BugB_SingletonNull() {
    std::cout << "\n[BUG B] 싱글턴 인스턴스 미검사\n";
    std::cout << "  GameManager가 생성되기 전에 접근합니다...\n";

    // BAD: instance가 nullptr인데 바로 사용!
    GameManager::instance->AddScore(100);  // CRASH!
    std::cout << "  점수: " << GameManager::instance->score << "\n";
}

/*
 * BUG C: 컴포넌트 체인 호출
 * - GetComponent가 nullptr을 반환할 수 있는데 검사 없이 체인 호출
 * - GetTransform()->GetX()에서 GetTransform()이 nullptr이면 크래시
 */
void BugC_ComponentChain(SceneSystem& scene) {
    std::cout << "\n[BUG C] 컴포넌트 체인 호출\n";
    std::cout << "  Transform이 없는 오브젝트에서 위치를 가져옵니다...\n";

    // "EmptyObj"에는 Transform이 추가되지 않았음!
    GameObject* obj = scene.FindByName("EmptyObj");
    if (obj) {
        // BAD: GetTransform()이 nullptr을 반환하는데 바로 ->GetX() 호출!
        float x = obj->GetTransform()->GetX();  // CRASH!
        std::cout << "  위치: " << x << "\n";
    }
}

/*
 * BUG D: 맵에서 없는 키 접근 후 역참조
 * - map[key]는 키가 없으면 기본값(nullptr)을 삽입하고 반환
 * - 반환된 nullptr을 바로 역참조
 */
void BugD_MapDefaultNull() {
    std::cout << "\n[BUG D] 맵에서 없는 키 접근 후 역참조\n";
    std::cout << "  존재하지 않는 Entity ID로 조회합니다...\n";

    std::map<int, GameObject*> entityMap;
    entityMap[1] = new GameObject("Entity_1");

    // BAD: entityId 999는 맵에 없음 -> operator[]가 nullptr 삽입 후 반환
    int entityId = 999;
    GameObject* entity = entityMap[entityId];
    std::cout << "  엔티티 이름: " << entity->GetName() << "\n";  // CRASH!

    // cleanup
    for (auto& [k, v] : entityMap) delete v;
}

// ============================================================================
// 메인 - 메뉴 시스템
// ============================================================================
int main() {
    // 씬 세팅
    SceneSystem scene;
    auto* hero = new GameObject("Hero");
    hero->AddTransform();
    hero->AddRenderer();
    scene.Register("Hero", hero);

    // Transform 없는 빈 오브젝트
    auto* empty = new GameObject("EmptyObj");
    scene.Register("EmptyObj", empty);

    std::cout << "====================================================\n";
    std::cout << "  ZeroCrashLab - 01. Null Pointer Dereference\n";
    std::cout << "  (널 포인터 역참조)\n";
    std::cout << "====================================================\n";
    std::cout << "\n  이 프로그램은 의도적으로 크래시를 발생시킵니다.\n";
    std::cout << "  교육생은 크래시 원인을 파악하고 코드를 수정하세요.\n\n";
    std::cout << "  [A] Find/Get 체인 반환값 미검사\n";
    std::cout << "  [B] 싱글턴 인스턴스 미검사\n";
    std::cout << "  [C] 컴포넌트 체인 호출\n";
    std::cout << "  [D] 맵에서 없는 키 접근 후 역참조\n";
    std::cout << "  [Q] 종료\n";
    std::cout << "----------------------------------------------------\n";

    while (true) {
        std::cout << "\n선택> ";
        char choice;
        std::cin >> choice;
        choice = toupper(choice);

        switch (choice) {
        case 'A': BugA_FindGetChain(scene); break;
        case 'B': BugB_SingletonNull(); break;
        case 'C': BugC_ComponentChain(scene); break;
        case 'D': BugD_MapDefaultNull(); break;
        case 'Q': std::cout << "종료합니다.\n"; return 0;
        default:  std::cout << "잘못된 입력입니다.\n"; break;
        }
    }
}
