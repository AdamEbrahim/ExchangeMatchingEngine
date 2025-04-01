#ifndef CUSTOM_EXCEPTION_H
#define CUSTOM_EXCEPTION_H

#include <exception>
#include <string>

class CustomException : public std::exception {
private:
    std::string message;

public:
    explicit CustomException(const std::string& msg);
    const char* what() const noexcept override;
};

#endif