#include "core/profiling.hpp"
namespace ve::prof {
Accumulator& Accumulator::instance(){ static Accumulator inst; return inst; }
void Accumulator::add(Sample s){ std::scoped_lock l(mtx_); samples_.push_back(std::move(s)); }
std::vector<Sample> Accumulator::snapshot(){ std::scoped_lock l(mtx_); return samples_; }
ScopedTimer::~ScopedTimer(){ auto end = Clock::now(); double ms = std::chrono::duration<double,std::milli>(end-start_).count(); Accumulator::instance().add(Sample{std::string(name_), ms}); }
} // namespace ve::prof