#pragma once
#include <string>

namespace ve::fx {

class Effect {
public:
    virtual ~Effect() = default;
    virtual std::string type() const = 0;
};

} // namespace ve::fx
