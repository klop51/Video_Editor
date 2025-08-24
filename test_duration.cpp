#include "media_io/media_probe.hpp"
#include "core/log.hpp"
#include <iostream>

int main(int argc, char* argv[]) {
    ve::log::set_level(ve::log::Level::Info);
    
    if (argc != 2) {
        std::cout << "Usage: test_duration <media-file>" << std::endl;
        return 1;
    }
    
    std::string file_path = argv[1];
    
    // Test the media probe functionality
    auto probe_result = ve::media::probe_file(file_path);
    
    if (probe_result.success) {
        std::cout << "Media probe successful!" << std::endl;
        std::cout << "Duration: " << probe_result.duration_us << " us (" 
                  << (probe_result.duration_us / 1000000.0) << " seconds)" << std::endl;
        
        if (probe_result.duration_us > 0) {
            int64_t end_time = probe_result.duration_us - 1000; // End minus 1ms
            std::cout << "Go to end target: " << end_time << " us" << std::endl;
            std::cout << "✅ Duration extraction working correctly!" << std::endl;
        } else {
            std::cout << "❌ Duration is 0 - this is the issue!" << std::endl;
        }
    } else {
        std::cout << "❌ Media probe failed!" << std::endl;
    }
    
    return 0;
}
