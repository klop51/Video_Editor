# core Module
Responsibility: Foundational types (time, rational, logging), lightweight utilities shared by all other modules.
Key Dependencies: fmt, spdlog.
Provides: time abstractions (TimePoint/Duration), logging facade (core/log.hpp).
Notes: Keep free of heavy external libs to minimize rebuild ripple.
