//
// Created by 김남주 on 2023/05/30.
//

#include "System.hpp"
#include "Error.hpp"
#include <cassert>
#include <limits>
#include <algorithm>

page_replacement_policy str_to_policy(const std::string& policy_str) {
    if (policy_str == LRU_STRING) {
        return LRU;
    } else if (policy_str == FIFO_STRING) {
        return FIFO;
    } else if (policy_str == LFU_STRING) {
        return LFU;
    } else if (policy_str == MFU_STRING) {
        return MFU;
    }

    fprintf(stderr, "Not valid policy\n");
    throw RunException("Not valid policy\n");
}

PhysicalFrame::PhysicalFrame(int process_id, int page_id, int fi_score, int fu_score, int ru_score) {
    this->process_id = process_id;
    this->page_id = page_id;
    this->fi_score = fi_score;
    this->fu_score = fu_score;
    this->ru_score = ru_score;

    linked_page = nullptr;
}

//PhysicalFrame::~PhysicalFrame() {
//    for (auto& linked_page: linked_pages) {
//        if (linked_page == nullptr) continue;
//        delete linked_page;
//        linked_page = nullptr;
//    }
//}

Process::Process(std::string name, int pid, int ppid, process_state state, int next_allocation_id, int next_page_id) {
this->name = std::move(name);
this->pid = pid;
this->ppid = ppid;
this->state = state;
this->next_allocation_id = next_allocation_id;
this->next_page_id = next_page_id;
// -1은 아무것도 할당되지 않음을 의미
virtual_memory.assign(VIRTUAL_MEMORY_SIZE, -1);
page_table.assign(VIRTUAL_MEMORY_SIZE, nullptr);
}

PageTableEntry::PageTableEntry(int physical_address, int allocation_id, char authority) {
    this->physical_address = physical_address;
    this->allocation_id = allocation_id;
    this->authority = authority;
}


int Status::free_memory_size() const {
    int remaining_physical_memory_size = 0;

    for (const auto& m: this->physical_memory) {
        if (m == nullptr) remaining_physical_memory_size++;
    }

    return remaining_physical_memory_size;
}

std::vector<int> Status::free_memory_addresses(int num) const {

    if (this->free_memory_size() < num || num == 0) {
        return {};
    }

    std::vector<int> addresses;
    addresses.reserve(PHYSICAL_MEMORY_SIZE);

    for (int i = 0; i < PHYSICAL_MEMORY_SIZE; i++) {
        if (this->physical_memory[i] == nullptr) {
            addresses.push_back(i);
        }
        if (addresses.size() == num) break;
    }

    assert(addresses.size() == num);
    return addresses;
}

void Status::replace_page() {
    int min_score = std::numeric_limits<int>::max();
    int max_score = -1;
    int iteration = 0;

    // 교체 되어야 할 물리 메모리 인덱스
    int replace_index = -1;

    switch (this->replacement_policy) {
        case FIFO:
            for (auto& m : this->physical_memory) {
                if (m == nullptr) {iteration++; continue;}
                if (m->fi_score < min_score) {
                    min_score = m->fi_score;
                    replace_index = iteration;
                }
                iteration++;
            }
            break;
        case LRU:
            for (auto& m : this->physical_memory) {
                if (m == nullptr) {iteration++; continue;}
                if (m->ru_score < min_score) {
                    min_score = m->ru_score;
                    replace_index = iteration;
                }
                iteration++;
            }
            break;
        case LFU:
            for (auto& m : this->physical_memory) {
                if (m == nullptr) {iteration++; continue;}
                if (m->fu_score < min_score) {
                    min_score = m->fu_score;
                    replace_index = iteration;
                }
                iteration++;
            }
            break;
        case MFU:
            for (auto& m : this->physical_memory) {
                if (m == nullptr) {iteration++; continue;}
                if (m->fu_score > max_score) {
                    max_score = m->fu_score;
                    replace_index = iteration;
                }
                iteration++;
            }
            break;
    }

    assert(replace_index != -1);

    // Paging out
    physical_memory[replace_index]->fu_score = 0;
    physical_memory[replace_index]->fi_score = 0;
    physical_memory[replace_index]->ru_score = 0;
    this->swap_space.push_back(physical_memory[replace_index]);
    this->physical_memory[replace_index] = nullptr;

    // 연결된 페이지 테이블 갱신
    this->swap_space.back()->linked_page->physical_address = -1;
}

std::vector<Process *> Status::get_child_processes(int parent_id) const {
    auto res = std::vector<Process*>();

    for (const auto pr: this->process_ready) {
        if (pr == nullptr) continue;
        if (pr->ppid == parent_id) res.push_back(pr);
    }

    for (const auto pw: this->process_waiting) {
        if (pw == nullptr) continue;
        if (pw->ppid == parent_id) res.push_back(pw);
    }

    if (this->process_new != nullptr) {
        if (this->process_new->ppid == parent_id) res.push_back(this->process_new);
    }
    if (this->process_running != nullptr) {
        if (this->process_running->ppid == parent_id) res.push_back(this->process_running);
    }

    return res;
}

Process *Status::get_process_by_pid(int pid) const {
    Process* p = nullptr;

    for (const auto pr: this->process_ready) {
        if (pr == nullptr) continue;
        if (pr->pid == pid) return pr;
    }

    for (const auto pw: this->process_waiting) {
        if (pw == nullptr) continue;
        if (pw->pid == pid) return pw;
    }

    if (this->process_new != nullptr) {
        if (this->process_new->pid == pid) return this->process_new;
    }
    if (this->process_running != nullptr) {
        if (this->process_running->pid == pid) return this->process_running;
    }

    return p;
}


