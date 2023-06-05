//
// Created by 김남주 on 2023/06/03.
//

#include "System.hpp"
#include "Run.hpp"
#include <algorithm>
#include <cassert>

using namespace Run;

void page_fault_handler(int page_id) {
    Process* p = status.process_running;

    int virtual_address = std::distance(p->virtual_memory.begin(),
                                        std::find(p->virtual_memory.begin(),
                                                  p->virtual_memory.end(),
                                                  page_id));
    auto& target_pe = p->page_table[virtual_address];
    int target_frame_pid;
    if (target_pe->authority == 'R') {
        target_frame_pid = p->ppid;
    } else {
        target_frame_pid = p->pid;
    }

    // 물리 메모리에 공간이 없다면 페이지 교체
    if (status.free_memory_size() <= 0) {
        status.replace_page();
    }
    int physical_address_to_allocate = status.free_memory_addresses(1).front();

    // 스왑 영역에서 프레임 찾고 물리 메모리에 할당
    int swap_address = -1;
    PhysicalFrame* frame;
    for (size_t i = 0; i < status.swap_space.size(); i++) {
        frame = status.swap_space[i];
        if (frame == nullptr) continue;
        if (frame->page_id == page_id && frame->process_id == target_frame_pid) {
            swap_address = i;
            status.physical_memory[physical_address_to_allocate] = frame;
            frame->fi_score = status.top_fi_score++;
            frame->fu_score++;
            frame->ru_score = status.top_ru_score++;
            for (auto& linked_pe:frame->linked_pages) {
                if((*linked_pe) == nullptr) continue;
                (*linked_pe)->physical_address = physical_address_to_allocate;
            }
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
    Process* p = status.process_running;

    int virtual_address = std::distance(p->virtual_memory.begin(),
                                        std::find(p->virtual_memory.begin(),
                                                  p->virtual_memory.end(),
                                                  page_id));
    auto& target_pe = p->page_table[virtual_address];

    // 공유하고 있던 프레임을 찾아내서 역참조 제거
    PhysicalFrame* shared_frame;
    if (target_pe->physical_address == -1) {
        // 스왑 영역에 있는 경우
        for (const auto& frame: status.swap_space) {
            if (frame == nullptr) continue;
            if (frame->process_id == p->ppid && frame->page_id == page_id) {
                shared_frame = frame;
                break;
            }
        }
    } else {
        shared_frame = status.physical_memory[target_pe->physical_address];
    }

    const auto remove_it = std::remove_if(shared_frame->linked_pages.begin(),
                                       shared_frame->linked_pages.end(),
                                       [&](const auto& p) {return *p == target_pe;});
    shared_frame->linked_pages.erase(remove_it);

    if (status.free_memory_size() <= 0) {
        status.replace_page();
    }
    int physical_address_to_allocate = status.free_memory_addresses(1).front();

    const auto& copied_new_frame = status.physical_memory[physical_address_to_allocate]
            = new PhysicalFrame(p->pid, page_id,
                                                                                                            0, 0, 0);
    copied_new_frame->fi_score = status.top_fi_score++;
    copied_new_frame->fu_score++;
    copied_new_frame->ru_score = status.top_ru_score++;
    copied_new_frame->linked_pages.push_back(&(target_pe));

    target_pe->authority = 'W';
    target_pe->physical_address = physical_address_to_allocate;

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