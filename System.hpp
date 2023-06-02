//
// Created by 김남주 on 2023/05/26.
//

#ifndef HW3_SYSTEM_HPP
#define HW3_SYSTEM_HPP

#include <string>
#include <vector>
#include "Syscall.hpp"
#include "Fault.hpp"

// kString
const std::string KERNEL_MODE_STRING = "kernel";
const std::string USER_MODE_STRING = "user";

const std::string BOOT_COMMAND_STRING = "boot";
const std::string RUN_COMMAND_STRING = "run";
const std::string SLEEP_COMMAND_STRING = "sleep";
const std::string WAIT_COMMAND_STRING = "wait";
const std::string EXIT_COMMAND_STRING = "exit";
const std::string FORK_AND_EXEC_COMMAND_STRING = "fork_and_exec";
const std::string MEMORY_ALLOCATE_COMMAND_STRING = "memory_allocate";
const std::string MEMORY_RELEASE_COMMAND_STRING = "memory_release";
const std::string MEMORY_READ_COMMAND_STRING = "memory_read";
const std::string MEMORY_WRITE_COMMAND_STRING = "memory_write";
const std::string SCHEDULE_COMMAND_STRING = "schedule";
const std::string IDLE_COMMAND_STRING = "idle";
const std::string SYSTEM_CALL_COMMAND_STRING = "system call";
const std::string FAULT_COMMAND_STRING = "fault";

const std::string LRU_STRING = "lru";
const std::string FIFO_STRING = "fifo";
const std::string MFU_STRING = "mfu";
const std::string LFU_STRING = "lfu";

const int VIRTUAL_MEMORY_SIZE = 32;
const int PHYSICAL_MEMORY_SIZE = 16;
const int SWAP_SPACE_SIZE = 100;

enum process_state {
    New,
    Waiting,
    Ready,
    Running,
    Terminated,
};

enum page_replacement_policy {
    FIFO,
    LRU,
    MFU,
    LFU,
};

page_replacement_policy str_to_policy(const std::string& policy_str);

struct PageTableEntry {
    int physical_address;
    int allocation_id;
    char authority; // W or R;

    PageTableEntry(int physical_address, int allocation_id, char authority = 'W');
};

struct PhysicalFrame {
    int process_id;
    int page_id;

    /**
     * 프레임과 연결된 페이지
     */
    std::vector<PageTableEntry*> linked_pages = std::vector<PageTableEntry*>();

    //score에 따라 알고리즘에 의해 replace 될 메모리

    // 높을수록 최근에 접근된 메모리
    int ru_score;
    // 높을수록 최근에 삽입된 메모리
    int fi_score;
    // 높을수록 많이 접근된 메모리
    int fu_score;

    /**
     * 생성자
     * @param process_id
     * @param page_id
     */
    PhysicalFrame(int process_id, int page_id, int fi_score = 0, int fu_score = 0, int ru_score = 0);
};

// todo: 메모리 관련 attribute 추가
struct Process {
    std::string name; // 프로세스 이름
    int pid; // 프로세스 ID
    int ppid; // 부모 프로세스 ID
    char waiting_type = '\0'; // waiting일 때 S, W중 하나
    process_state state; // process_state
    int remain_sleep_time = 0; // 남은 sleep time
    int current_line = 1; // 현재 읽고 있는 명령어 줄
    std::vector<int> virtual_memory;
    std::vector<PageTableEntry*> page_table;
    int next_allocation_id;
    int next_page_id;

    /**
     * 생성자
     * @param name 프로그램 이름
     * @param pid  프로세스 id
     * @param ppid 부모 프로세스 id
     * @param state 프로세스 상태
     * @param next_allocation_id 프로세스의 다음 메모리 할당 id
     */
    Process(std::string name, int pid, int ppid, process_state state = New, int next_allocation_id = 0,
            int last_page_id = 0);
};

struct Status {
    int cycle;
    std::string mode;
    std::string command;
    Process* process_running;
    std::vector<Process*> process_ready; // ready queue
    std::vector<Process*> process_waiting;
    Process* process_new;
    Process* process_terminated;
    system_call_type syscall_type;
    fault_type fault_type;
    std::string syscall_arg;
    std::vector<PhysicalFrame*> physical_memory = std::vector<PhysicalFrame*>(PHYSICAL_MEMORY_SIZE, nullptr);
    std::vector<PhysicalFrame*> swap_space = std::vector<PhysicalFrame*>();
    page_replacement_policy replacement_policy;

    int process_num = 0;

    int top_ru_score = 0;
    int top_fi_score = 0;
    int top_fu_score = 0;

    /**
     * 남은 물리 메모리 공간을 프레임 단위로 반환
     * @return 물리 메모리에 남은 프레임 수
     */
    int free_memory_size() const;

    /**
     * 남는 공간의 메모리 주소(인덱스)를 오름차순으로 반환
     * @param num 반환할 메모리 주소의 수
     * @return 메모리 주소의 std::vector
     */
    std::vector<int> free_memory_addresses(int num);

    /**
     * 페이지 교체
     * @param num_of_replace 교체되어야 할 페이지 수
     */
    void replace_page();

    std::vector<Process*> get_child_processes(int parent_id);
};

#endif //HW3_SYSTEM_HPP
