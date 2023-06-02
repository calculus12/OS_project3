//
// Created by 김남주 on 2023/05/26.
//

#ifndef HW3_SYSCALL_HPP
#define HW3_SYSCALL_HPP

#include <string>

enum system_call_type {
    Sleep,
    Wait,
    Fork_and_exec,
    Exit,
    Memory_allocate,
    Memory_release,
};

/**
 * sleep function
 * @param p sleep할 프로세스
 * @param sleep_time sleep 시간
 */
void sleep(int sleep_time);

/**
 * fork_and_exec
 * @param p
 * @param program_name
 */
void fork_and_exec(std::string program_name);

/**
 * wait
 */
void wait();

/**
 * exit
 * @param p
 */
void exit();

/**
 * cycle 0 에서 실행되는 boot
 */
void boot();

/**
 * schedule function
 */
void schedule();

/**
 * 시스템 콜이 아닐 떄, schedule 또는 idle 실행
 */
void schedule_or_idle();

/**
 * 메모리 할당 함수
 * @param allocation_size 할당할 메모리 크기 (페이지 개수)
 */
void memory_allocate(int allocation_size);

/**
 * 메모리 해제 함수
 * @param allocation_id 해제할 메모리 allocation id
 */
void memory_release(int allocation_id);

/**
 * status의 system_call_command 를 보고 시스템 콜을 처리
 */
void system_call();

system_call_type string_to_system_call_type(const std::string& str);

#endif //HW3_SYSCALL_HPP
