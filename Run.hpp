//
// Created by 김남주 on 2023/05/26.
//

#ifndef HW3_RUN_HPP
#define HW3_RUN_HPP

#include "System.hpp"

const int PRINT_FRAME_UNIT = 4;

namespace Run {
    // 결과 출력 파일 스트림
    extern FILE* result_file;
    // Global Status
    extern Status status;
    // Executing directory
    extern std::string path;
}


std::vector<std::string> split(const std::string& str, char delim = ' ');

/**
 * 현재 프로세스의 명령어를 읽고 명령어를 리턴
 * @param path 파일 경로
 * @param file_name 파일 이름
 * @param status status
 * @return 읽은 명령어
 */
std::string run_program();

/**
 * 명령 실행 전 업데이트\n
 * 1. sleep시간 갱신\n
 * 2. 프로세스 상태 갱신\n
 * 3. ready queue 갱신\n
 * @param status status 변수
 */
void update();

/**
 * 1 cycle 실행 (run일때는 argument만큼 cycle 실행)
 * @param status
 * @param path
 */
void perform_cycle();

/**
 * 커널 시뮬레이터 실행
 */
void run(const std::string& run_path, const std::string& replacement_policy, const std::string& result_filename = "result");

void print_status();

#endif //HW3_RUN_HPP
