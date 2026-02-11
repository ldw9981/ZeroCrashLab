/*============================================================================
 *  ZeroCrashLab - 05. Division by Zero / INF / NaN
 *  ---------------------------------------------------------------------------
 *  수학 연산에서 0으로 나누거나, 결과가 무한대/NaN이 되어
 *  후속 연산을 오염시키는 유형입니다.
 *  - 정수 나누기 0: 즉시 하드웨어 예외 → 크래시
 *  - 부동소수점 나누기 0: inf 또는 NaN 생성 → 오브젝트 소실/텔레포트
 *
 *  [교육 목표] 0 나누기가 발생하는 상황을 파악하고 안전한 처리를 추가하세요.
 *============================================================================*/
#include <iostream>
#include <cmath>
#include <limits>

// ============================================================================
// 간이 구조체
// ============================================================================
struct Vector3 {
    float x, y, z;
    Vector3(float x = 0, float y = 0, float z = 0) : x(x), y(y), z(z) {}

    float LengthSquared() const { return x * x + y * y + z * z; }
    float Length() const { return sqrtf(LengthSquared()); }

    void Normalize() {
        float len = Length();
        x /= len; y /= len; z /= len;  // len == 0이면 NaN!
    }

    void Print(const char* label) const {
        std::cout << "    " << label << ": (" << x << ", " << y << ", " << z << ")\n";
    }
};

// ============================================================================
// BUG A: 기본값 0인 멤버 변수가 분모
// - fps = 0.0f로 초기화된 변수로 나누기
// ============================================================================
class SpriteSheet {
public:
    float fps = 0.0f;       // 기본값 0!
    int frameCount = 12;

    float GetDuration() {
        // BAD: fps == 0이면 Division by Zero!
        return frameCount / fps;
    }
};

class Camera {
public:
    int screenWidth = 800;
    int screenHeight = 0;   // 기본값 0! (아직 초기화 안 됨)

    float GetAspectRatio() {
        // BAD: screenHeight == 0이면 Division by Zero!
        return (float)screenWidth / (float)screenHeight;
    }
};

void BugA_DefaultZeroDenominator() {
    std::cout << "\n[BUG A] 기본값 0인 멤버 변수가 분모\n";

    std::cout << "  --- SpriteSheet ---\n";
    SpriteSheet sprite;
    std::cout << "  fps = " << sprite.fps << ", frameCount = " << sprite.frameCount << "\n";
    float duration = sprite.GetDuration();
    std::cout << "  duration = " << duration << " (inf!)\n";

    std::cout << "\n  --- Camera ---\n";
    Camera cam;
    std::cout << "  width = " << cam.screenWidth << ", height = " << cam.screenHeight << "\n";
    float aspect = cam.GetAspectRatio();
    std::cout << "  aspect = " << aspect << " (inf!)\n";

    std::cout << "\n  [결과] inf 값이 후속 계산을 모두 오염시킵니다!\n";
    std::cout << "  프로젝션 행렬이 inf → 모든 렌더링이 깨집니다.\n";
}

// ============================================================================
// BUG B: 정수 0 나누기 (즉시 크래시!)
// ============================================================================
void BugB_IntegerDivisionByZero() {
    std::cout << "\n[BUG B] 정수 0 나누기 (즉시 크래시!)\n";
    std::cout << "  정수 나누기에서 0으로 나누면 하드웨어 예외가 발생합니다...\n\n";

    int totalEnemies = 100;
    int teamCount = 0;  // 팀이 없는 경우!

    std::cout << "  totalEnemies = " << totalEnemies << "\n";
    std::cout << "  teamCount = " << teamCount << "\n";
    std::cout << "  enemiesPerTeam 계산 중...\n";

    // BAD: 정수 0 나누기 → 즉시 크래시!
    int enemiesPerTeam = totalEnemies / teamCount;  // CRASH!
    std::cout << "  enemiesPerTeam = " << enemiesPerTeam << "\n";
}

// ============================================================================
// BUG C: NaN 전파 (영벡터 정규화)
// ============================================================================
void BugC_NaNPropagation() {
    std::cout << "\n[BUG C] NaN 전파 (영벡터 정규화)\n";
    std::cout << "  영벡터를 정규화하면 NaN이 생성되고 모든 연산을 오염시킵니다.\n\n";

    Vector3 position(100.0f, 50.0f, 200.0f);
    Vector3 direction(0.0f, 0.0f, 0.0f);  // 이동하지 않는 상태 → 영벡터

    position.Print("초기 위치");
    direction.Print("이동 방향 (영벡터!)");

    // BAD: 영벡터 정규화 → NaN!
    direction.Normalize();
    direction.Print("정규화 후 (NaN!)");

    // NaN과의 모든 연산 결과도 NaN
    float speed = 10.0f;
    position.x += direction.x * speed;
    position.y += direction.y * speed;
    position.z += direction.z * speed;

    position.Print("이동 후 위치 (NaN!)");

    // NaN 검증
    std::cout << "\n  isnan(position.x) = " << std::isnan(position.x) << "\n";
    std::cout << "  [결과] 위치가 NaN이 되어 오브젝트가 화면에서 사라집니다!\n";
}

// ============================================================================
// BUG D: 거리 계산에서 매우 작은 값
// ============================================================================
void BugD_NearZeroDistance() {
    std::cout << "\n[BUG D] 거리 계산에서 매우 작은 값\n";
    std::cout << "  두 오브젝트가 거의 같은 위치에 있을 때 역수 계산이 폭발합니다.\n\n";

    Vector3 playerPos(10.0f, 0.0f, 10.0f);
    Vector3 enemyPos(10.0f, 0.0f, 10.0f);  // 플레이어와 같은 위치!

    float dx = playerPos.x - enemyPos.x;
    float dz = playerPos.z - enemyPos.z;
    float distSq = dx * dx + dz * dz;

    std::cout << "  플레이어 위치: (" << playerPos.x << ", " << playerPos.z << ")\n";
    std::cout << "  적 위치:       (" << enemyPos.x << ", " << enemyPos.z << ")\n";
    std::cout << "  distSq = " << distSq << "\n";

    // BAD: distSq가 0이면 sqrtf(0) = 0, 1.0/0 = inf!
    float invDist = 1.0f / sqrtf(distSq);
    std::cout << "  1/sqrt(distSq) = " << invDist << " (inf!)\n";

    // 방향 벡터가 inf → 적이 순간이동
    float dirX = dx * invDist;
    float dirZ = dz * invDist;
    std::cout << "  direction = (" << dirX << ", " << dirZ << ") (NaN!)\n";
    std::cout << "  [결과] 적이 NaN 위치로 순간이동합니다!\n";
}

// ============================================================================
// 메인
// ============================================================================
int main() {
    std::cout << "====================================================\n";
    std::cout << "  ZeroCrashLab - 05. Division by Zero / INF / NaN\n";
    std::cout << "====================================================\n";
    std::cout << "\n  이 프로그램은 0 나누기와 NaN 전파를 시연합니다.\n";
    std::cout << "  교육생은 원인을 파악하고 안전한 처리를 추가하세요.\n\n";
    std::cout << "  [A] 기본값 0인 멤버 변수가 분모 (inf 생성)\n";
    std::cout << "  [B] 정수 0 나누기 (즉시 크래시!)\n";
    std::cout << "  [C] NaN 전파 (영벡터 정규화)\n";
    std::cout << "  [D] 거리 계산에서 매우 작은 값\n";
    std::cout << "  [Q] 종료\n";
    std::cout << "----------------------------------------------------\n";

    while (true) {
        std::cout << "\n선택> ";
        char choice;
        std::cin >> choice;
        choice = toupper(choice);

        switch (choice) {
        case 'A': BugA_DefaultZeroDenominator(); break;
        case 'B': BugB_IntegerDivisionByZero(); break;
        case 'C': BugC_NaNPropagation(); break;
        case 'D': BugD_NearZeroDistance(); break;
        case 'Q': std::cout << "종료합니다.\n"; return 0;
        default:  std::cout << "잘못된 입력입니다.\n"; break;
        }
    }
}
