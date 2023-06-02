#include "Run.hpp"

using namespace std;

int main(int argc, char* argv[]) {
    // 마지막에 '/'가 붙지 않아도 실행할 수 있도록 변경 => 입력에 디렉토리가 표시가 되어야 함 ex) /home/test/programs
    string path = string(argv[argc - 2]) + "/";
    string replacement_policy = string(argv[argc - 1]);

    run(path, replacement_policy);
    return 0;
}


