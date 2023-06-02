//
// Created by 김남주 on 2023/06/03.
//

#ifndef HW3_FAULT_HPP
#define HW3_FAULT_HPP

enum fault_type {
    Page_fault,
    Protection_fault,
    None,
};

/**
 * 페이지 플트 핸들러
 * @param page_id 물리 메모리로 가져와야 하는 page_id
 */
void page_fault_handler(int page_id);

/**
 * 프로텍션 폴트 핸들러
 * @param page_id 읽기 권한만 있는 page_id
 */
void protection_fault_handler(int page_id);

void fault_handler();

#endif //HW3_FAULT_HPP
