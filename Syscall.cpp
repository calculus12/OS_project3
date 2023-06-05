//
// Created by 김남주 on 2023/05/26.
//

#include "Syscall.hpp"
#include "System.hpp"
#include "Run.hpp"
#include "Error.hpp"
#include <algorithm>

using namespace Run;

void sleep(int sleep_time) {
    Process *p = status.process_running;
    p->state = Waiting;
    p->remain_sleep_time = sleep_time - 1;

    if (p->remain_sleep_time == 0) {
        status.process_ready.push_back(p);
        p->state = Ready;
    } else {
        status.process_waiting.push_back(p);
        p->waiting_type = 'S';
    }

    status.process_running = nullptr;
}

void fork_and_exec(std::string program_name) {
    Process *p = status.process_running;

    auto *new_process = new Process(std::move(program_name), status.process_num + 1, p->pid, New,
                                    p->next_allocation_id, p->next_page_id);
    status.process_new = new_process;

    // 부모 프로세스의 페이지 및 가상 메모리 CoW 형식으로 복사
    for (int address = 0; address < VIRTUAL_MEMORY_SIZE; address++) {

        const auto &parent_pe = p->page_table[address];
        if (parent_pe == nullptr) continue;
        // 가상 메모리 복사
        new_process->virtual_memory[address] = p->virtual_memory[address];

        // 부모 프로세스의 페이지도 읽기 권한으로 변경
        parent_pe->authority = 'R';
        // 페이지 테이블 엔트리 복사됨
        new_process->page_table[address] = new PageTableEntry(parent_pe->physical_address, parent_pe->allocation_id,
                                                              'R');

        // 역참조 추가
        if (parent_pe->physical_address != -1) {
            // 해당 프레임이 물리 메모리에 있는 경우
            const auto& frame = status.physical_memory[parent_pe->physical_address];
            frame->linked_pages.push_back(&(new_process->page_table[address]));
        } else {
            // 스왑 영역에 있는 경우
            for (const auto& frame: status.swap_space) {
                if (frame->process_id == p->pid && frame->page_id == p->virtual_memory[address]) {
                    frame->linked_pages.push_back(&(new_process->page_table[address]));
                    break;
                }
            }
        }
    }

    status.process_num++;

    // 부모 프로세스는 다시 Ready
    status.process_ready.push_back(p);
    p->state = Ready;
    status.process_running = nullptr;
}

void wait() {
    Process *p = status.process_running;

    bool exist_child_process = false;

    // new process 검사
    if (status.process_new != nullptr) {
        if (status.process_new->ppid == p->pid) {
            exist_child_process = true;
        }
    }

    // waiting queue 검사
    for (Process *wp: status.process_waiting) {
        if (wp->ppid == p->pid) {
            exist_child_process = true;
            break;
        }
    }

    // ready queue 검사
    for (Process *rp: status.process_ready) {
        if (rp->ppid == p->pid) {
            exist_child_process = true;
            break;
        }
    }

    if (exist_child_process) {
        status.process_waiting.push_back(p);
        p->state = Waiting;
        p->waiting_type = 'W';
    } else {
        status.process_ready.push_back(p);
        p->waiting_type = '\0';
        p->state = Ready;
    }

    status.process_running = nullptr;
}

void exit() {
    Process *p = status.process_running;

    p->state = Terminated;

    // waiting 하고 있는 부모 프로세스의 상태를 Ready로 변경
    for (auto it = status.process_waiting.begin(); it != status.process_waiting.end(); ++it) {
        auto pp = *it;
        if (p->ppid == pp->pid) {
            if (pp->waiting_type == 'W') {
                pp->state = Ready;
                pp->waiting_type = '\0';
                status.process_ready.push_back(pp);
                break;
            }
        }
    }

    // Ready Queue에 들어간 프로세스들을 wating queue에서 제거
    auto it = std::remove_if(status.process_waiting.begin(), status.process_waiting.end(),
                             [](Process *p) { return p->state == Ready; });
    status.process_waiting.erase(it, status.process_waiting.end());

    // 해당 프로세스에 할당된 물리 메모리를 모두 해제
    for (int i = 0; i < VIRTUAL_MEMORY_SIZE; i++) {
        if (p->virtual_memory[i] == -1) continue;
        PhysicalFrame** target_frame;
        auto &pe = p->page_table[i];

        int target_frame_pid = p->pid;
        if (pe->authority == 'R') target_frame_pid = p->ppid == 0 ? 1 : p->ppid;

        if (pe->physical_address == -1) {
            // 스왑 영역에 있는 경우
            for (auto &frame_in_swap_space: status.swap_space) {
                if (frame_in_swap_space == nullptr) continue;
                if (frame_in_swap_space->process_id == target_frame_pid && frame_in_swap_space->page_id == p->virtual_memory[i]) {
                    target_frame = &frame_in_swap_space;
                    break;
                }
            }
        } else {
            // 물리 메모리에 있는 경우
            target_frame = &status.physical_memory[pe->physical_address];
        }
        // 역참조 제거
        const auto remove_it = std::remove((*target_frame)->linked_pages.begin(),
                                           (*target_frame)->linked_pages.end(),
                                           &pe);
        (*target_frame)->linked_pages.erase(remove_it);

        // 쓰기 권한까지 있을 떄 물리 메모리에서 제거
        if (pe->authority == 'W' || p->pid == 1) {
            delete (*target_frame);
            (*target_frame) = nullptr;
        }

        delete pe;
        pe = nullptr;
    }

    // 스왑 영역에서 nullptr이거 된 프레임 제거
    const auto swap_it = std::remove_if(status.swap_space.begin(), status.swap_space.end(),
                                        [](const auto &frame) { return frame == nullptr; });
    status.swap_space.erase(swap_it, status.swap_space.end());

    status.process_running = nullptr;
    status.process_terminated = p;
}

void boot() {
    status.command = BOOT_COMMAND_STRING;
    auto *init = new Process("init", 1, 0);
    status.process_new = init;
    status.process_num++;
}

void schedule() {
    status.command = SCHEDULE_COMMAND_STRING;
    Process *p = status.process_ready.front();
    p->state = Running;

    // running process로 바꾸고 ready queue에서 지운다.
    status.process_running = p;
    status.process_ready.erase(status.process_ready.begin());
}

void schedule_or_idle() {
    if (!status.process_ready.empty()) {
        schedule();
    } else {
        status.command = IDLE_COMMAND_STRING;
    }
}

void memory_allocate(int allocation_size) {

    Process *p = status.process_running;

    // 연속적으로 할당될 가상 메모리 시작 주소
    size_t allocate_begin_index = 0;

    // 물리 메모리에 공간이 부족한 경우 할당할 수 있을 때까지 페이지 교체 반복
    int replace_num = allocation_size - status.free_memory_size();
    if (replace_num > 0) {
        // 페이지 교체
        for (int i = 0; i < replace_num; i++) {
            status.replace_page();
        }
    }

    // 할당될 가상 메모리 공간 찾기
    for (int i = 0; i < VIRTUAL_MEMORY_SIZE; i++) {
        bool can_allocate = true;
        for (size_t j = i; j < i + allocation_size; j++) {
            if (p->virtual_memory[j] != -1) {
                can_allocate = false;
                break;
            }
        }
        if (!can_allocate) continue;
        allocate_begin_index = i;
        // 가상 메모리에 할당
        for (int j = i; j < i + allocation_size; j++) {
            p->virtual_memory[j] = p->next_page_id + j - i;
        }
        break;
    }


    // 페이지 테이블 갱신 및 물리 메모리에 할당
    auto allocation_addresses_array = status.free_memory_addresses(allocation_size);

    for (const auto &address: allocation_addresses_array) {
        p->page_table[allocate_begin_index] = new PageTableEntry(address,
                                                                 p->next_allocation_id);
        status.physical_memory[address] = new PhysicalFrame(p->pid, p->next_page_id++,
                                                            status.top_fi_score, 1, status.top_ru_score);

        // 나중에 역참조 정보를 갱신하기 위해 reference 로 전달
        status.physical_memory[address]->linked_pages.push_back(&(p->page_table[allocate_begin_index]));
        allocate_begin_index++;
    }

    p->next_allocation_id++;
    status.top_fi_score++;
    status.top_ru_score++;
    // 처리가 끝난 후 레디 큐 삽입
    p->state = Ready;
    status.process_ready.push_back(p);
    status.process_running = nullptr;
}

// todo 공유하고 있는 자식 프로세스에서 memory release가 일어나나면 다른 자식 프로세스도 W 권한을 부여함과 동시에 새로운 프레임을 만들어야 하나?
// todo 즉, 부모 프로세스의 페이지에 대해 공유하고 있는 자식 프로세스가 2개 이상일 때 어떻게 처리해야 하나?
// todo, 런어스 질문 https://ys.learnus.org/mod/ubboard/article.php?id=3107595&bwid=1141254 답변 기다리기
void memory_release(int allocation_id) {
    Process *p = status.process_running;


    // 가상 메모리 및 물리 메모리에서 제거
    for (int virtual_address = 0; virtual_address < VIRTUAL_MEMORY_SIZE; virtual_address++) {
        auto &pe = p->page_table[virtual_address];
        if (pe == nullptr) continue;
        if (pe->allocation_id != allocation_id) continue;

        // 가상 메모리에서 페이지 제거
        int released_page_id = p->virtual_memory[virtual_address];
        p->virtual_memory[virtual_address] = -1;

        PhysicalFrame** target_frame;
        int target_frame_pid = p->pid;
        if (pe->authority == 'R' || p->pid != 1) {
            target_frame_pid = p->ppid;
        }

        // 물리 메모리에서 해제
        if (pe->physical_address == -1) {
            // 스왑 영역에 있는 경우
            for (auto &frame_in_swap_space: status.swap_space) {
                if (frame_in_swap_space == nullptr) continue;
                if (frame_in_swap_space->process_id == target_frame_pid &&
                    frame_in_swap_space->page_id == released_page_id) {
                    target_frame = &frame_in_swap_space;
                    break;
                }
            }
        } else {
            target_frame = &status.physical_memory[pe->physical_address];
        }

        // 역참조 제거
        const auto remove_it = std::remove((*target_frame)->linked_pages.begin(),
                                           (*target_frame)->linked_pages.end(),
                                           &pe);
        (*target_frame)->linked_pages.erase(remove_it);

        // 쓰기 권한까지 있을 때 물리 메모리에서 제거
        if (pe->authority == 'W' || p->pid == 1) {
            delete (*target_frame);
            (*target_frame) = nullptr;
        }

        delete pe;
        pe = nullptr;
    }

    // 스왑 영역에서 nullptr이 된 프레임 제거
    const auto swap_it = std::remove(status.swap_space.begin(), status.swap_space.end(),
                                        nullptr);
    status.swap_space.erase(swap_it, status.swap_space.end());

    // 해제한 메모리를 공유하고 있는 자식 프로세스에 페이지 및 프레임 복사
    auto child_processes = status.get_child_processes(p->pid);

    for (auto &child: child_processes) {
        for (int virtual_address = 0; virtual_address < VIRTUAL_MEMORY_SIZE; virtual_address++) {
            auto &pe = child->page_table[virtual_address];
            if (pe == nullptr) continue;
            // 공유하고 있던 페이지를(read 권한만 있던) 부모 페이지로부터 복사 (write 권한을 부여 하고 물리 메모리에 생성)
            if (pe->allocation_id == allocation_id &&
                pe->authority == 'R') {
                // 복사하고 스왑영역에 넣어 놓기
                status.swap_space.push_back(new PhysicalFrame(child->pid,
                                                              child->virtual_memory[virtual_address],
                                                              status.top_fi_score++));
                status.swap_space.back()->linked_pages.push_back(&pe);
                pe->physical_address = -1;
                pe->authority = 'W';
            }
        }
    }

    // 처리가 끝난 후 레디 큐 삽입
    p->state = Ready;
    status.process_ready.push_back(p);
    status.process_running = nullptr;

}


void system_call() {
    int num_arg;
    switch (status.syscall_type) {
        case Sleep:
            num_arg = stoi(status.syscall_arg);
            sleep(num_arg);
            break;
        case Wait:
            wait();
            break;
        case Exit:
            exit();
            break;
        case Fork_and_exec:
            fork_and_exec(status.syscall_arg);
            break;
        case Memory_allocate:
            num_arg = stoi(status.syscall_arg);
            memory_allocate(num_arg);
            break;
        case Memory_release:
            num_arg = stoi(status.syscall_arg);
            memory_release(num_arg);
            break;
        default:
            break;
    }
}

system_call_type string_to_system_call_type(const std::string &str) {
    if (str == SLEEP_COMMAND_STRING) {
        return system_call_type::Sleep;
    } else if (str == WAIT_COMMAND_STRING) {
        return system_call_type::Wait;
    } else if (str == EXIT_COMMAND_STRING) {
        return system_call_type::Exit;
    } else if (str == FORK_AND_EXEC_COMMAND_STRING) {
        return system_call_type::Fork_and_exec;
    } else if (str == MEMORY_ALLOCATE_COMMAND_STRING) {
        return system_call_type::Memory_allocate;
    } else if (str == MEMORY_RELEASE_COMMAND_STRING) {
        return system_call_type::Memory_release;
    }

    fprintf(stderr, "Argument does not match system call command\n");
    throw RunException("Argument does not match system call command\n");
}
