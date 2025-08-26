#pragma once

// Lightweight expected<T,E> implementation (subset) to support exception-free APIs in
// performance sensitive modules. Intentionally minimal: no monadic ops, no reference
// support yet. Error type E must be trivially copyable or movable.
// Inspired by std::expected (C++23) semantics where applicable.

#include <utility>
#include <type_traits>
#include <new>

namespace ve {

struct unexpect_t { explicit unexpect_t() = default; };
inline constexpr unexpect_t unexpect{};

template <class E>
class unexpected {
public:
    static_assert(!std::is_reference_v<E>, "unexpected<E&> not supported");
    constexpr explicit unexpected(const E& e) : error_(e) {}
    constexpr explicit unexpected(E&& e) : error_(std::move(e)) {}
    constexpr const E& error() const & noexcept { return error_; }
    constexpr E& error() & noexcept { return error_; }
    constexpr E&& error() && noexcept { return std::move(error_); }
private:
    E error_;
};

template <class T, class E>
class expected {
public:
    static_assert(!std::is_reference_v<T>, "expected<T&> not supported");
    static_assert(!std::is_reference_v<E>, "expected<E&> not supported");

    // Constructors
    constexpr expected() noexcept(std::is_nothrow_default_constructible_v<T>) : has_(true) {
        ::new (&storage_.value_) T();
    }
    constexpr expected(const T& v) : has_(true) { ::new (&storage_.value_) T(v); }
    constexpr expected(T&& v) noexcept(std::is_nothrow_move_constructible_v<T>) : has_(true) { ::new (&storage_.value_) T(std::move(v)); }
    constexpr expected(const unexpected<E>& ue) : has_(false) { ::new (&storage_.error_) E(ue.error()); }
    constexpr expected(unexpected<E>&& ue) noexcept(std::is_nothrow_move_constructible_v<E>) : has_(false) { ::new (&storage_.error_) E(std::move(ue.error())); }

    constexpr expected(unexpect_t, const E& e) : has_(false) { ::new (&storage_.error_) E(e); }
    constexpr expected(unexpect_t, E&& e) noexcept(std::is_nothrow_move_constructible_v<E>) : has_(false) { ::new (&storage_.error_) E(std::move(e)); }

    // Copy
    expected(const expected& other) : has_(other.has_) {
        if(has_) ::new (&storage_.value_) T(other.storage_.value_);
        else ::new (&storage_.error_) E(other.storage_.error_);
    }
    // Move
    expected(expected&& other) noexcept(std::is_nothrow_move_constructible_v<T> && std::is_nothrow_move_constructible_v<E>) : has_(other.has_) {
        if(has_) ::new (&storage_.value_) T(std::move(other.storage_.value_));
        else ::new (&storage_.error_) E(std::move(other.storage_.error_));
    }

    // Destructor
    ~expected() { destroy(); }

    // Assignment (basic strong-ish guarantee via copy+swap pattern simplified)
    expected& operator=(const expected& rhs) {
        if(this == &rhs) return *this;
        if(has_ && rhs.has_) { storage_.value_ = rhs.storage_.value_; }
        else if(!has_ && !rhs.has_) { storage_.error_ = rhs.storage_.error_; }
        else { destroy(); has_ = rhs.has_; if(has_) ::new (&storage_.value_) T(rhs.storage_.value_); else ::new (&storage_.error_) E(rhs.storage_.error_); }
        return *this;
    }
    expected& operator=(expected&& rhs) noexcept(std::is_nothrow_move_assignable_v<T> && std::is_nothrow_move_assignable_v<E>) {
        if(this == &rhs) return *this;
        if(has_ && rhs.has_) { storage_.value_ = std::move(rhs.storage_.value_); }
        else if(!has_ && !rhs.has_) { storage_.error_ = std::move(rhs.storage_.error_); }
        else { destroy(); has_ = rhs.has_; if(has_) ::new (&storage_.value_) T(std::move(rhs.storage_.value_)); else ::new (&storage_.error_) E(std::move(rhs.storage_.error_)); }
        return *this;
    }

    // Observers
    constexpr bool has_value() const noexcept { return has_; }
    explicit constexpr operator bool() const noexcept { return has_; }
    constexpr const T& value() const & { return storage_.value_; }
    constexpr T& value() & { return storage_.value_; }
    constexpr T&& value() && { return std::move(storage_.value_); }
    constexpr const E& error() const & { return storage_.error_; }
    constexpr E& error() & { return storage_.error_; }

private:
    union Storage { T value_; E error_; Storage(){} ~Storage(){} } storage_;
    bool has_{false};

    void destroy() noexcept {
        if(has_) storage_.value_.~T(); else storage_.error_.~E();
    }
};

// Helper factory
template <class E>
unexpected<E> make_unexpected(E e) { return unexpected<E>(std::move(e)); }

} // namespace ve
