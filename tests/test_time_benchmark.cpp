#include <catch2/catch_test_macros.hpp>
#include "core/time.hpp"
#include "core/profiling.hpp"
#include <vector>
#include <random>

TEST_CASE("Time comparison micro benchmark", "[time][benchmark]") {
    using namespace ve;
    // Generate rational pairs with various denominators
    std::vector<TimeRational> values; values.reserve(1000);
    for(int i=1;i<=1000;++i){ values.push_back(TimeRational{ i*100, (i%7)+1 }); }
    size_t compares = 0;
    {
        VE_PROFILE_SCOPE("time_compare_loop");
        for(int iter=0; iter<1000; ++iter){
            for(size_t a=0;a<values.size();++a){
                for(size_t b=a+1;b<values.size();++b){
                    bool lt = values[a] < values[b]; (void)lt; ++compares;
                }
            }
        }
    }
    // Ensure loop executed
    REQUIRE(compares > 1000000);
    // Write profiling artifact (not asserted, just smoke test)
    auto ok = ve::prof::Accumulator::instance().write_json("profiling_test.json");
    REQUIRE(ok);
}