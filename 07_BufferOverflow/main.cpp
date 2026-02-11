/*============================================================================
 *  ZeroCrashLab - 07. Buffer Overflow / Out-of-Bounds Access
 *  ---------------------------------------------------------------------------
 *  배열이나 버퍼의 유효 범위를 벗어나 접근하여 메모리 손상 또는 크래시.
 *  - 빈 벡터에 [0] 접근
 *  - Off-by-one 경계 검사 (> vs >=)
 *  - 고정 크기 배열 오버플로
 *  - 외부 데이터 인덱스 미검증
 *
 *  [교육 목표] 범위 밖 접근을 유발하는 코드를 찾고 올바른 경계 검사를 추가하세요.
 *============================================================================*/
#include <iostream>
#include <vector>
#include <string>
#include <cstring>
#include <cstdio>

// ============================================================================
// BUG A: 빈 벡터 접근
// ============================================================================
void BugA_EmptyVectorAccess() {
    std::cout << "\n[BUG A] 빈 벡터 접근\n";
    std::cout << "  빈 벡터에서 [0], .back(), .front()를 호출하면 UB입니다.\n\n";

    std::vector<float> vertices;  // 비어있음!
    std::cout << "  vertices.size() = " << vertices.size() << " (비어있음!)\n";

    // BAD: 빈 벡터에서 &[0] 접근 → UB!
    std::cout << "  &vertices[0] 접근 시도...\n";
    float* ptr = &vertices[0];  // CRASH!
    std::cout << "  ptr = " << ptr << "\n";
    std::cout << "  이 메시지는 보이지 않을 것입니다.\n";
}

// ============================================================================
// BUG B: Off-by-One 경계 검사
// ============================================================================
void BugB_OffByOne() {
    std::cout << "\n[BUG B] Off-by-One 경계 검사\n";
    std::cout << "  > 대신 >= 를 사용해야 하는데 빠뜨리면 범위 밖 접근.\n\n";

    std::vector<std::string> cameras = {"Main", "UI", "Debug"};
    std::cout << "  cameras.size() = " << cameras.size() << "\n";

    auto SetMainCamera = [&](size_t index) {
        // BAD: > 대신 >= 를 사용해야 함!
        if (index > cameras.size()) return;  // index == size() 일 때 통과!
        std::cout << "  cameras[" << index << "] = " << cameras[index] << "\n";
    };

    std::cout << "  index 0: "; SetMainCamera(0);
    std::cout << "  index 1: "; SetMainCamera(1);
    std::cout << "  index 2: "; SetMainCamera(2);
    std::cout << "  index 3 (== size()): ";
    SetMainCamera(3);  // cameras[3] → OOB!
}

// ============================================================================
// BUG C: 고정 크기 배열 오버플로
// ============================================================================
struct BonePoseBuffer {
    float bonePose[4][16];  // 4개의 본만 수용 (4x4 행렬)
};

void BugC_FixedArrayOverflow() {
    std::cout << "\n[BUG C] 고정 크기 배열 오버플로\n";
    std::cout << "  고정 크기 배열에 제한 없이 쓰면 스택이 손상됩니다.\n\n";

    BonePoseBuffer buffer;
    int boneCount = 10;  // 실제 본 수가 배열 크기를 초과!

    std::cout << "  배열 크기: 4, 실제 본 수: " << boneCount << "\n";
    std::cout << "  배열 경계를 넘어서 쓰기 시작...\n";

    // BAD: boneCount > 4이면 배열 경계를 넘어서 씀!
    for (int i = 0; i < boneCount; i++) {
        for (int j = 0; j < 16; j++) {
            buffer.bonePose[i][j] = (float)(i * 16 + j);  // i >= 4 → 스택 오버플로!
        }
        std::cout << "    bone[" << i << "] 기록 완료\n";
    }
    std::cout << "  이 메시지는 보이지 않을 것입니다 (스택 손상).\n";
}

// ============================================================================
// BUG D: 문자열 + 정수 → 포인터 산술 (잘못된 오류 메시지)
// ============================================================================
void BugD_StringPlusInt() {
    std::cout << "\n[BUG D] 문자열 리터럴 + 정수 = 포인터 산술!\n";
    std::cout << "  \"text\" + int 은 문자열 연결이 아니라 포인터 이동입니다.\n\n";

    int boneIndex = 5;

    // BAD: "text" + int → 포인터를 int만큼 전진!
    // "Bone index: "는 const char[13]이므로 +5하면 "ndex: "가 됨
    const char* msg = "Bone index: " + boneIndex;
    std::cout << "  의도: \"Bone index: 5\"\n";
    std::cout << "  실제: \"" << msg << "\"\n";

    // 큰 인덱스면 문자열 범위 밖 읽기!
    int bigIndex = 100;
    std::cout << "\n  index = " << bigIndex << " 으로 시도...\n";
    // BAD: "Bone index not Found" + 100 → 문자열 범위 밖!
    const char* errMsg = "Bone index not Found" + bigIndex;  // OOB!
    std::cout << "  errMsg = " << errMsg << "\n";  // 가비지 또는 크래시
}

// ============================================================================
// BUG E: wchar 버퍼 오버플로 (문자열 연결)
// ============================================================================
void BugE_WcharBufferOverflow() {
    std::cout << "\n[BUG E] 고정 크기 문자열 버퍼 오버플로\n";
    std::cout << "  두 개의 256자 버퍼를 연결하면 512자 → 첫 버퍼 오버플로!\n\n";

    char prefix[32];
    char message[256];

    // prefix를 긴 내용으로 채우기
    sprintf_s(prefix, 32, "[ERROR] MyVeryLongFunction:");

    // message를 긴 내용으로 채우기
    sprintf_s(message, 256,
        "This is a very long error message that contains lots of details "
        "about what went wrong in the system, including variable names, "
        "values, and stack trace information that is very useful for "
        "debugging but makes the string extremely long.");

    std::cout << "  prefix 길이: " << strlen(prefix) << "\n";
    std::cout << "  message 길이: " << strlen(message) << "\n";
    std::cout << "  prefix 버퍼 크기: 32\n";

    // BAD: prefix(32자 버퍼) + message(200+ 자) → 버퍼 오버플로!
    // strcat_s는 안전하지만, 공간이 부족하면 assertion 실패
    std::cout << "  strcat_s로 연결 시도...\n";
    strcat_s(prefix, 32, message);  // CRASH! 버퍼 오버플로!
    std::cout << "  이 메시지는 보이지 않을 것입니다.\n";
}

// ============================================================================
// 메인
// ============================================================================
int main() {
    std::cout << "====================================================\n";
    std::cout << "  ZeroCrashLab - 07. Buffer Overflow / OOB Access\n";
    std::cout << "====================================================\n";
    std::cout << "\n  이 프로그램은 범위 밖 접근 상황을 시연합니다.\n";
    std::cout << "  교육생은 올바른 경계 검사를 추가하세요.\n\n";
    std::cout << "  [A] 빈 벡터 접근\n";
    std::cout << "  [B] Off-by-One 경계 검사\n";
    std::cout << "  [C] 고정 크기 배열 오버플로\n";
    std::cout << "  [D] 문자열 + 정수 = 포인터 산술\n";
    std::cout << "  [E] 고정 크기 문자열 버퍼 오버플로\n";
    std::cout << "  [Q] 종료\n";
    std::cout << "----------------------------------------------------\n";

    while (true) {
        std::cout << "\n선택> ";
        char choice;
        std::cin >> choice;
        choice = toupper(choice);

        switch (choice) {
        case 'A': BugA_EmptyVectorAccess(); break;
        case 'B': BugB_OffByOne(); break;
        case 'C': BugC_FixedArrayOverflow(); break;
        case 'D': BugD_StringPlusInt(); break;
        case 'E': BugE_WcharBufferOverflow(); break;
        case 'Q': std::cout << "종료합니다.\n"; return 0;
        default:  std::cout << "잘못된 입력입니다.\n"; break;
        }
    }
}
