//
// Created by 김남주 on 2023/05/26.
//

#include "Run.hpp"
#include "Syscall.hpp"
#include "Fault.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>

namespace Run {
    // 결과 출력 파일 스트림
    FILE *result_file;
    // Global Status
    Status status;
    // Executing directory
    std::string path;
}

using namespace Run;

const bool OUTPUT_STDOUT = true;

std::vector<std::string> split(const std::string &str, char delim) {
    std::vector<std::string> result;
    std::istringstream ss(str);
    std::string token;
    while (std::getline(ss, token, delim)) {
        result.push_back(token);
    }
    return result;
}

std::string run_program() {
    std::ifstream program(path + status.process_running->name);
    std::string command;

    for (int i = 1; i < status.process_running->current_line; i++) {
        std::string line;
        getline(program, line);
    }

    std::getline(program, command);
    program.close();

    // 다음 줄로 이동
    status.process_running->current_line++;
    return command;
}

void update() {

    for (Process *p: status.process_waiting) {
        // sleep 시간 갱신, 상태 갱신 (waiting -> ready)
        if (p->remain_sleep_time > 0 && p->waiting_type == 'S') {
            if (--(p->remain_sleep_time) == 0) {
                p->state = Ready;
            }
        }
    }

    // Ready Queue 갱신 (waiting -> ready)
    for (int i = 0; i < status.process_waiting.size(); i++) {
        auto p = status.process_waiting[i];
        if (p->state == Ready) {
            status.process_ready.push_back(p);
            p->waiting_type = '\0';
        }
    }

    // Ready Queue에 들어간 프로세스들을 wating queue에서 제거
    auto it = std::remove_if(status.process_waiting.begin(), status.process_waiting.end(),
                             [](Process *p) { return p->state == Ready; });
    status.process_waiting.erase(it, status.process_waiting.end());


    // 상태 갱신 (new -> ready), Ready queue 삽입
    if (status.process_new != nullptr) {
        status.process_new->state = Ready;
        status.process_ready.push_back(status.process_new);
        status.process_new = nullptr;
    }

    // 상태 갱신 (terminated 삭제)
    if (status.process_terminated != nullptr) {
        delete status.process_terminated;
        status.process_terminated = nullptr;
    }
}

void perform_cycle() {
    update();

    // 커널 모드 일때
    if (status.mode == KERNEL_MODE_STRING) {
        if (status.command == SYSTEM_CALL_COMMAND_STRING) {
            // 시스템 콜 수행
            system_call();

            // 상태 출력 후 모드 스위칭
            print_status();
            status.mode = KERNEL_MODE_STRING;
            status.command = "";
        } else if (status.command == FAULT_COMMAND_STRING) {
            // 폴트 핸들러 수행
            fault_handler();

            // 상태 출력 후 모드 스위칭
            print_status();
            status.mode = KERNEL_MODE_STRING;
            status.command = "";
        } else {
            // 스케쥴 또는 idle 실행
            schedule_or_idle();

            // 상태 출력 후 모드 스위칭
            print_status();
            status.mode = USER_MODE_STRING;
        }
    } else {
        // 유저 모드일때
        status.command = run_program();
        auto command_vector = split(status.command);
        std::string command = command_vector[0];
        std::string argument;
        if (command_vector.size() > 1) {
            argument = command_vector[1];
        }


        if (command == RUN_COMMAND_STRING) {
            // 명령어가 run인 경우
            int arg_num = stoi(argument);
            for (int i = 0; i < arg_num; i++) {
                update();
                print_status();
                status.cycle++;
            }
            return;
        } else if (command == MEMORY_READ_COMMAND_STRING) {
            // 명령어가 memory_read인 경우
            print_status();
            Process* p = status.process_running;
            int page_id_to_read = stoi(argument);
            int virtual_address = std::distance(p->virtual_memory.begin(),
                                                std::find(p->virtual_memory.begin(),
                                                          p->virtual_memory.end(),
                                                          page_id_to_read));
            auto& target_page_table_entry = p->page_table[virtual_address];

            if (target_page_table_entry->physical_address == -1) {
                // 물리 메모리에 없다면 페이지 퐅트 핸들러 호출
                status.command = FAULT_COMMAND_STRING;
                status.mode = KERNEL_MODE_STRING;
                status.fault_handler_type = Page_fault;
                status.syscall_arg = argument;
            } else {
                // ru(recently used), fu(frequently used) 점수 갱신
                auto& target_frame = status.physical_memory[target_page_table_entry->physical_address];
                target_frame->ru_score = status.top_ru_score++;
                target_frame->fu_score++;
            }

        } else if (command == MEMORY_WRITE_COMMAND_STRING) {
            // 명령어가 memory_write인 경우
            print_status();
            Process* p = status.process_running;
            int page_id_to_write = stoi(argument);
            int virtual_address = std::distance(p->virtual_memory.begin(),
                                                std::find(p->virtual_memory.begin(),
                                              p->virtual_memory.end(),
                                              page_id_to_write));
            auto& target_page_table_entry = p->page_table[virtual_address];

            if (target_page_table_entry->authority == 'R') {
                // 읽기 권한만 있을 때
                status.command = FAULT_COMMAND_STRING;
                status.mode = KERNEL_MODE_STRING;
                status.fault_handler_type = Protection_fault;
                status.syscall_arg = argument;
            } else {
                // 쓰기 권한이 있을 때
                if (target_page_table_entry->physical_address == -1) {
                    status.command = FAULT_COMMAND_STRING;
                    status.mode = KERNEL_MODE_STRING;
                    status.fault_handler_type = Page_fault;
                    status.syscall_arg = argument;
                } else {
                    auto& target_frame = status.physical_memory[target_page_table_entry->physical_address];
                    target_frame->ru_score = status.top_ru_score++;
                    target_frame->fu_score++;
                }
            }
        } else {
            // 시스템 콜을 호출하는 경우
            print_status();
            status.command = SYSTEM_CALL_COMMAND_STRING;
            status.mode = KERNEL_MODE_STRING;
            status.syscall_type = string_to_system_call_type(command);
            status.syscall_arg = argument;
        }
    }


    // cycle +1
    status.cycle++;
//    if (status.command == SYSTEM_CALL_COMMAND_STRING) {
//        status.command = "";
//    }
}

void run(const std::string &run_path, const std::string &replacement_policy, const std::string &result_filename) {
    status = Status();
    // 페이지 교체 알고리즘 설정
    status.replacement_policy = str_to_policy(replacement_policy);
    Run::path = run_path;
    result_file = OUTPUT_STDOUT ? stdout : fopen(result_filename.c_str(), "w");

    status.mode = KERNEL_MODE_STRING;

    // Boot 호출 (cycle 0)
    boot();
    print_status();
    status.cycle++;

    // cycle 1부터 시작
    while (true) {
        perform_cycle();

        // 종료되는 프로세스가 init (종료 조건) -> 탈출
        if (status.process_terminated != nullptr) {
            if (status.process_terminated->pid == 1) {
                delete status.process_terminated;
                status.process_terminated = nullptr;
                break;
            }
        }
    }

    fclose(result_file);
}

void print_status() {
    // 0. 몇번째 cycle인지
    fprintf(result_file, "[cycle #%d]\n", status.cycle);

    // 1. 현재 실행 모드 (user or kernel)
    fprintf(result_file, "1. mode: %s\n", status.mode.c_str());

    // 2. 현재 실행 명령어
    fprintf(result_file, "2. command: %s\n", status.command.c_str());

    // 3. 현재 실행중인 프로세스의 정보. 없을 시 none 출력
    if (status.process_running == nullptr) {
        fprintf(result_file, "3. running: none\n");
    } else {
        Process *i = status.process_running;
        fprintf(result_file, "3. running: %d(%s, %d)\n", i->pid, i->name.c_str(), i->ppid);
    }

    // 4. 현재 물리 메모리 상황
    fprintf(result_file, "4. physical memory:\n");

    fprintf(result_file, "|");
    for (int i = 0; i < PHYSICAL_MEMORY_SIZE; i++) {
        if (status.physical_memory[i] == nullptr) {
            fprintf(result_file, "-");
        } else {
            fprintf(result_file, "%d(%d)", status.physical_memory[i]->process_id, status.physical_memory[i]->page_id);
        }

        if (i % PRINT_FRAME_UNIT == PRINT_FRAME_UNIT - 1) {
            fprintf(result_file, "|");
        } else {
            fprintf(result_file, " ");
        }
    }
    fprintf(result_file, "\n");

    // 현재 Running 상태의 프로세스가 없으면 아래 정보는 출력되지 않는다.
    if (status.process_running == nullptr) {
        fprintf(result_file, "\n");
        return;
    }

    // 5. 현재 실행중인 프로세스의 가상 메모리 상황
    fprintf(result_file, "5. virtual memory:\n");

    fprintf(result_file, "|");
    for (int i = 0; i < VIRTUAL_MEMORY_SIZE; i++) {
        if (status.process_running->virtual_memory[i] == -1) {
            fprintf(result_file, "-");
        } else {
            fprintf(result_file, "%d", status.process_running->virtual_memory[i]);
        }

        if (i % PRINT_FRAME_UNIT == PRINT_FRAME_UNIT - 1) {
            fprintf(result_file, "|");
        } else {
            fprintf(result_file, " ");
        }
    }
    fprintf(result_file, "\n");

    // 6. 현재 실행중인 프로세스의 페이지 테이블 상황
    fprintf(result_file, "6. page table:\n");

    // 페이지 테이블 매핑 정보
    fprintf(result_file, "|");
    for (int i = 0; i < VIRTUAL_MEMORY_SIZE; i++) {
        // 페이지 테이블 엔트리가 없거나 스왑 영역에 있을 때
        if (status.process_running->page_table[i] == nullptr
            || status.process_running->page_table[i]->physical_address == -1) {
            fprintf(result_file, "-");
        } else {
            fprintf(result_file, "%d", status.process_running->page_table[i]->physical_address);
        }

        if (i % PRINT_FRAME_UNIT == PRINT_FRAME_UNIT - 1) {
            fprintf(result_file, "|");
        } else {
            fprintf(result_file, " ");
        }
    }
    fprintf(result_file, "\n");

    // 페이지 테이블 권한 정보
    fprintf(result_file, "|");
    for (int i = 0; i < VIRTUAL_MEMORY_SIZE; i++) {
        // 페이지 테이블 엔트리가 없을때
        if (status.process_running->page_table[i] == nullptr) {
            fprintf(result_file, "-");
        } else {
            fprintf(result_file, "%c", status.process_running->page_table[i]->authority);
        }

        if (i % PRINT_FRAME_UNIT == PRINT_FRAME_UNIT - 1) {
            fprintf(result_file, "|");
        } else {
            fprintf(result_file, " ");
        }
    }
    fprintf(result_file, "\n");


    // 매 cycle 간의 정보는 두번의 개행으로 구분
    fprintf(result_file, "\n");
}