#pragma once

#include <string>
#include <variant>
#include <optional>

namespace ve::core {

/**
 * Result type for operations that can fail
 * Provides a safe way to handle success/error cases without exceptions
 */
template<typename T>
class Result {
public:
    // Constructors
    Result(T&& value) : data_(std::move(value)) {}
    Result(const T& value) : data_(value) {}
    Result(std::string error) : data_(std::move(error)) {}
    
    // Check if result is successful
    bool is_ok() const { return std::holds_alternative<T>(data_); }
    bool is_error() const { return std::holds_alternative<std::string>(data_); }
    
    // Access value (only call if is_ok())
    const T& value() const { return std::get<T>(data_); }
    T& value() { return std::get<T>(data_); }
    
    // Access error message (only call if is_error())
    const std::string& error() const { return std::get<std::string>(data_); }
    
    // Safe access
    std::optional<T> try_value() const {
        if (is_ok()) {
            return value();
        }
        return std::nullopt;
    }
    
    // Transform operations
    template<typename F>
    auto map(F&& func) -> Result<decltype(func(std::declval<T>()))> {
        using ReturnType = decltype(func(std::declval<T>()));
        
        if (is_ok()) {
            return Result<ReturnType>(func(value()));
        } else {
            return Result<ReturnType>(error());
        }
    }
    
private:
    std::variant<T, std::string> data_;
};

// Helper functions for creating Results
template<typename T>
Result<T> Ok(T&& value) {
    return Result<T>(std::forward<T>(value));
}

template<typename T>
Result<T> Error(std::string message) {
    return Result<T>(std::move(message));
}

// Void result type for operations that don't return values
using VoidResult = Result<bool>;

inline VoidResult Ok() {
    return VoidResult(true);
}

} // namespace ve::core
