#include "core/profiling.hpp"
#include <fstream>

// NOTE: <algorithm> intentionally omitted to avoid interaction with any stray macros
static_assert(sizeof(ve::prof::Sample) == sizeof(std::string) + sizeof(double), "Unexpected Sample layout change");

namespace ve::prof {

Accumulator& Accumulator::instance() {
	static Accumulator inst;
	return inst;
}

void Accumulator::add(Sample s) {
	std::scoped_lock lock(mtx_);
	samples_.push_back(std::move(s));
}

std::vector<Sample> Accumulator::snapshot() {
	std::scoped_lock lock(mtx_);
	return samples_;
}

std::unordered_map<std::string, Accumulator::Stats> Accumulator::aggregate() {
	std::scoped_lock lock(mtx_);

	std::unordered_map<std::string, std::vector<double>> buckets;
	buckets.reserve(samples_.size());
	for (const auto& s : samples_) {
		buckets[s.name].push_back(s.ms);
	}

	std::unordered_map<std::string, Stats> out;
	out.reserve(buckets.size());

	for (auto& kv : buckets) {
		const std::string& name = kv.first;
		auto& vec = kv.second;
		if (vec.empty()) continue;

		// Manual insertion sort to avoid toolchain issue with std::sort token corruption
		for (size_t i = 1; i < vec.size(); ++i) {
			double key = vec[i];
			size_t j = i;
			while (j > 0 && vec[j - 1] > key) {
				vec[j] = vec[j - 1];
				--j;
			}
			vec[j] = key;
		}

		Stats st;
		st.count = vec.size();
		st.min_ms = vec.front();
		st.max_ms = vec.back();
		double sum = 0.0;
		for (double v : vec) sum += v;
		st.total_ms = sum;
		st.avg_ms = sum / st.count;
		size_t idx50 = (st.count > 1) ? static_cast<size_t>(0.5 * double(st.count - 1)) : 0;
		size_t idx95 = (st.count > 1) ? static_cast<size_t>(0.95 * double(st.count - 1)) : 0;
		if (idx50 >= vec.size()) idx50 = vec.size() - 1;
		if (idx95 >= vec.size()) idx95 = vec.size() - 1;
		st.p50_ms = vec[idx50];
		st.p95_ms = vec[idx95];
		out.emplace(name, st);
	}

	return out;
}

void Accumulator::clear() {
	std::scoped_lock lock(mtx_);
	samples_.clear();
}

bool Accumulator::write_json(const std::string& path) {
	auto agg = aggregate();
	std::ofstream ofs(path, std::ios::trunc);
	if (!ofs) return false;
	ofs << "{\n  \"samples\": [\n";
	bool first = true;
	for (auto& [name, st] : agg) {
		if (!first) ofs << ",\n"; else first = false;
		ofs << "    { \"name\": \"" << name << "\", \"count\": " << st.count
			<< ", \"total_ms\": " << st.total_ms
			<< ", \"avg_ms\": " << st.avg_ms
			<< ", \"min_ms\": " << st.min_ms
			<< ", \"max_ms\": " << st.max_ms
			<< ", \"p50_ms\": " << st.p50_ms
			<< ", \"p95_ms\": " << st.p95_ms
			<< " }";
	}
	ofs << "\n  ]\n}\n";
	return true;
}

ScopedTimer::~ScopedTimer() {
	auto end = Clock::now();
	double ms = std::chrono::duration<double, std::milli>(end - start_).count();
	Accumulator::instance().add(Sample{ std::string(name_), ms });
}

} // namespace ve::prof