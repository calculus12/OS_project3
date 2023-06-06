//
// Created by 김남주 on 2023/06/03.
//

#include "System.hpp"
#include "Run.hpp"
#include <algorithm>
#include <cassert>

using namespace Run;

void page_fault_handler(int page_id) {
    Process *p = status.process_running;

    int virtual_address = std::distance(p->virtual_memory.begin(),
                                        std::find(p->virtual_memory.begin(),
                                                  p->virtual_memory.end(),
                                                  page_id));
    auto &target_pe = p->page_table[virtual_address];
    int target_frame_pid = p->pid;
    if (target_pe->authority == 'R' && p->pid != 1) {
        target_frame_pid = p->ppid;
    }

    // 물리 메모리에 공간이 없다면 페이지 교체
    if (status.free_memory_size() <= 0) {
        status.replace_page();
    }
    int physical_address_to_allocate = status.free_memory_addresses(1).front();

    // 스왑 영역에서 프레임 찾고 물리 메모리에 할당
    int swap_address = -1;
    PhysicalFrame *frame;
    for (size_t i = 0; i < status.swap_space.size(); i++) {
        frame = status.swap_space[i];
        if (frame == nullptr) continue;
        if (frame->page_id == page_id && frame->process_id == target_frame_pid) {
            swap_address = i;
            status.physical_memory[physical_address_to_allocate] = frame;
            frame->fi_score = status.top_fi_score++;
            frame->fu_score++;
            frame->ru_score = status.top_ru_score++;
            frame->linked_page->physical_address = physical_address_to_allocate;
            break;
        }
    }

    assert(swap_address != -1);
    status.swap_space.erase(status.swap_space.begin() + swap_address);

    status.fault_handler_type = None;

    // 처리가 끝난 후 레디 큐 삽입
    p->state = Ready;
    status.process_ready.push_back(p);
    status.process_running = nullptr;
}

void protection_fault_handler(int page_id) {
    Process *p = status.process_running;

    long virtual_address = std::distance(p->virtual_memory.begin(),
                                         std::find(p->virtual_memory.begin(),
                                                   p->virtual_memory.end(),
                                                   page_id));
    auto &target_pe = p->page_table[virtual_address];

    int target_frame_pid = p->ppid;
    if (p->pid == 1) target_frame_pid = p->pid;

    // 공유하고 있던 프레임 찾기
    PhysicalFrame *shared_frame;
    if (target_pe->physical_address == -1) {
        // 스왑 영역에 있는 경우
        for (const auto &frame: status.swap_space) {
            if (frame == nullptr) continue;
            if (frame->process_id == target_frame_pid && frame->page_id == page_id) {
                shared_frame = frame;
                break;
            }
        }
    } else {
        shared_frame = status.physical_memory[target_pe->physical_address];
    }

    auto parent_process = status.get_process_by_pid(shared_frame->process_id);
    auto child_processes = status.get_child_processes(shared_frame->process_id);


    // 자식 프로세스들에서 공유하고 있는 페이지 복사 (할당 x)
    for (auto &child: child_processes) {
        for (int i = 0; i < VIRTUAL_MEMORY_SIZE; i++) {
            if (child->virtual_memory[i] == page_id) {
                auto &pe = child->page_table[i];
                if (pe->authority != 'R') continue;
                pe = new PageTableEntry(-1, pe->allocation_id);
                status.swap_space.push_back(new PhysicalFrame(child->pid, page_id));
                status.swap_space.back()->linked_page = pe;
                break;
            }
        }
    }

    if (shared_frame->process_id != p->pid) {
        // 자식 프로세스로 인해 fault가 발생한 경우 해당 프레임 새로 할당
        if (status.free_memory_size() <= 0) {
            status.replace_page();
        }
        int physical_address_to_allocate = status.free_memory_addresses(1).front();

        PhysicalFrame *copied_new_frame = nullptr;
        int iteration = -1;
        for (const auto &frame: status.swap_space) {
            iteration++;
            if (frame == nullptr) continue;
            if (frame->process_id == p->pid && frame->page_id == page_id) {
                copied_new_frame = frame;
                break;
            }
        }

        assert(copied_new_frame != nullptr);

        status.swap_space.erase(status.swap_space.begin() + iteration);

        copied_new_frame->fi_score = status.top_fi_score++;
        copied_new_frame->fu_score++;
        copied_new_frame->ru_score = status.top_ru_score++;

        status.physical_memory[physical_address_to_allocate] = copied_new_frame;
        target_pe->physical_address = physical_address_to_allocate;
    }

    if (p->pid != 1) {
        for (int i = 0; i < VIRTUAL_MEMORY_SIZE; i++) {
            if (parent_process->virtual_memory[i] == page_id) {
                parent_process->page_table[i]->authority = 'W';
                break;
            }
        }
    } else {
        target_pe->authority = 'W';
    }

    status.fault_handler_type = None;

    // 처리가 끝난 후 레디 큐 삽입
    p->state = Ready;
    status.process_ready.push_back(p);
    status.process_running = nullptr;
}

void fault_handler() {
    int num_arg;
    switch (status.fault_handler_type) {
        case Page_fault:
            num_arg = stoi(status.syscall_arg);
            page_fault_handler(num_arg);
            break;
        case Protection_fault:
            num_arg = stoi(status.syscall_arg);
            protection_fault_handler(num_arg);
            break;
        default:
            break;
    }
}