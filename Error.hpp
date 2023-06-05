//
// Created by 김남주 on 2023/06/05.
//

#ifndef HW3_ERROR_HPP
#define HW3_ERROR_HPP

#include <exception>

class RunException : public std::exception {
private:
    std::string message;

public:
    RunException(std::string msg) : message(msg) {}
    std::string what () {
        return message;
    }
};

#endif //HW3_ERROR_HPP
