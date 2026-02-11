/*============================================================================
 *  ZeroCrashLab - 08. Race Condition / Thread Safety
 *  ---------------------------------------------------------------------------
 *  다중 스레드에서 동기화 없이 공유 데이터에 접근하면
 *  데이터 레이스, 데드락, 크래시가 발생합니다.
 *
 *  [교육 목표] 스레드 안전하지 않은 코드를 찾고 mutex/thread_local 등으로 수정하세요.
 *============================================================================*/
#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <random>
#include <atomic>
#include <chrono>
#include <sstream>
#include <cstdio>

// ============================================================================
// BUG A: static 버퍼를 여러 스레드가 공유
// ============================================================================
class com_exception {
    unsigned int result;
public:
    com_exception(unsigned int hr) : result(hr) {}

    const char* what() const noexcept {
        // BAD: static 로컬 버퍼 → 모든 스레드가 공유!
        static char s_str[64] = {};
        sprintf_s(s_str, "Failure with HRESULT of %08X", result);
        return s_str;
        // 스레드 A가 쓰는 동안 스레드 B가 읽으면 가비지 문자열!
    }
};

void BugA_StaticBufferShared() {
    std::cout << "\n[BUG A] static 버퍼를 여러 스레드가 공유\n";
    std::cout << "  static 로컬 변수는 모든 스레드가 동시에 사용합니다.\n\n";

    auto worker = [](int threadId, unsigned int errorCode) {
        com_exception ex(errorCode);
        for (int i = 0; i < 100; i++) {
            const char* msg = ex.what();
            // 다른 스레드가 동시에 what()을 호출하면 s_str이 덮어씌워짐!
            std::ostringstream oss;
            oss << "  Thread " << threadId << ": " << msg << "\n";
            std::cout << oss.str();
            std::this_thread::sleep_for(std::chrono::microseconds(1));
        }
    };

    std::vector<std::thread> threads;
    threads.emplace_back(worker, 1, 0x80070005);
    threads.emplace_back(worker, 2, 0x80004001);
    threads.emplace_back(worker, 3, 0x8000FFFF);

    for (auto& t : threads) t.join();
    std::cout << "\n  [결과] 각 스레드의 에러 코드가 뒤섞여서 출력됩니다!\n";
}

// ============================================================================
// BUG B: 공유 카운터 동기화 없음
// ============================================================================
void BugB_SharedCounterNoSync() {
    std::cout << "\n[BUG B] 공유 카운터에 동기화 없음\n";
    std::cout << "  여러 스레드가 동시에 같은 변수를 수정하면 값이 손실됩니다.\n\n";

    // BAD: 일반 int를 여러 스레드에서 동시 수정!
    int sharedScore = 0;
    const int ITERATIONS = 100000;

    auto addScore = [&]() {
        for (int i = 0; i < ITERATIONS; i++) {
            sharedScore++;  // 원자적이지 않음! Read-Modify-Write 경쟁!
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < 4; i++) {
        threads.emplace_back(addScore);
    }
    for (auto& t : threads) t.join();

    int expected = 4 * ITERATIONS;
    std::cout << "  기대값: " << expected << "\n";
    std::cout << "  실제값: " << sharedScore << "\n";
    std::cout << "  손실:   " << (expected - sharedScore) << "점\n";
    std::cout << "  [결과] 동기화 없이 공유 변수를 수정하면 값이 손실됩니다!\n";
}

// ============================================================================
// BUG C: static 랜덤 엔진 (스레드 안전하지 않음)
// ============================================================================
struct Vector3 {
    float x, y, z;
};

Vector3 PickRandomTarget(float range) {
    // BAD: static 랜덤 엔진은 thread-safe하지 않음!
    static std::mt19937 gen(42);
    std::uniform_real_distribution<float> dist(-range, range);
    return {dist(gen), 0, dist(gen)};
    // 여러 스레드가 동시에 호출하면 gen의 내부 상태가 손상됨!
}

void BugC_StaticRandomEngine() {
    std::cout << "\n[BUG C] static 랜덤 엔진 (스레드 안전하지 않음)\n";
    std::cout << "  mt19937은 thread-safe하지 않습니다.\n\n";

    std::atomic<int> nanCount{0};

    auto worker = [&](int threadId) {
        for (int i = 0; i < 10000; i++) {
            Vector3 target = PickRandomTarget(100.0f);
            if (std::isnan(target.x) || std::isnan(target.z) ||
                std::isinf(target.x) || std::isinf(target.z) ||
                target.x > 200.0f || target.z > 200.0f ||
                target.x < -200.0f || target.z < -200.0f) {
                nanCount++;
            }
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < 4; i++) {
        threads.emplace_back(worker, i);
    }
    for (auto& t : threads) t.join();

    std::cout << "  비정상 값 발생 횟수: " << nanCount.load() << " / 40000\n";
    std::cout << "  [결과] 랜덤 엔진 내부 상태가 손상되어 비정상 값이 생성됩니다!\n";
}

// ============================================================================
// BUG D: 벡터 동시 push_back (데이터 레이스)
// ============================================================================
void BugD_VectorRaceCondition() {
    std::cout << "\n[BUG D] 벡터 동시 push_back (데이터 레이스)\n";
    std::cout << "  여러 스레드가 동시에 같은 vector에 push_back하면 크래시합니다.\n\n";

    // BAD: mutex 없이 여러 스레드에서 vector push_back!
    std::vector<int> sharedLog;

    auto worker = [&](int threadId) {
        for (int i = 0; i < 10000; i++) {
            sharedLog.push_back(threadId * 100000 + i);
            // vector 재할당이 동시에 발생하면 메모리 손상!
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < 4; i++) {
        threads.emplace_back(worker, i);
    }
    for (auto& t : threads) t.join();

    std::cout << "  기대 크기: " << 40000 << "\n";
    std::cout << "  실제 크기: " << sharedLog.size() << "\n";
    std::cout << "  [결과] 크기가 맞지 않거나, 중간에 크래시가 발생할 수 있습니다!\n";
}

// ============================================================================
// 메인
// ============================================================================
int main() {
    std::cout << "====================================================\n";
    std::cout << "  ZeroCrashLab - 08. Race Condition / Thread Safety\n";
    std::cout << "====================================================\n";
    std::cout << "\n  이 프로그램은 스레드 안전성 문제를 시연합니다.\n";
    std::cout << "  교육생은 동기화 문제를 찾고 수정하세요.\n\n";
    std::cout << "  [A] static 버퍼 공유 (문자열 레이스)\n";
    std::cout << "  [B] 공유 카운터 동기화 없음 (값 손실)\n";
    std::cout << "  [C] static 랜덤 엔진 (내부 상태 손상)\n";
    std::cout << "  [D] 벡터 동시 push_back (데이터 레이스)\n";
    std::cout << "  [Q] 종료\n";
    std::cout << "----------------------------------------------------\n";

    while (true) {
        std::cout << "\n선택> ";
        char choice;
        std::cin >> choice;
        choice = toupper(choice);

        switch (choice) {
        case 'A': BugA_StaticBufferShared(); break;
        case 'B': BugB_SharedCounterNoSync(); break;
        case 'C': BugC_StaticRandomEngine(); break;
        case 'D': BugD_VectorRaceCondition(); break;
        case 'Q': std::cout << "종료합니다.\n"; return 0;
        default:  std::cout << "잘못된 입력입니다.\n"; break;
        }
    }
}
