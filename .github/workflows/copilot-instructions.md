# GitHub Copilot Instructions

## Core Development Principles

### Code Quality & Security
- **NEVER** expose sensitive information (API keys, passwords, connection strings) in code
- **ALWAYS** validate user inputs and sanitize data before processing
- **ALWAYS** use proper error handling and avoid silent failures
- **ALWAYS** follow the principle of least privilege for permissions and access
- **NEVER** commit commented-out code blocks - remove them entirely
- **ALWAYS** prefer explicit, readable code over "clever" one-liners

### Project-Specific Guidelines

#### Technology Stack Adherence
- This is a **C++ video editor** with **Qt6** UI and **CMake** build system
- **NEVER** suggest Python, JavaScript, or other language solutions for core functionality
- **ALWAYS** use modern C++17/20 features when appropriate
- **ALWAYS** follow Qt6 patterns and best practices for UI components
- **ALWAYS** respect the existing CMake structure and presets

#### Architecture Consistency
- **NEVER** break existing namespace structure (`app::`, `core::`, `gpu::`, etc.)
- **ALWAYS** maintain separation between core logic and UI components
- **ALWAYS** use proper RAII patterns for resource management
- **ALWAYS** follow the established error handling patterns
- **NEVER** introduce singleton patterns without explicit justification

### Common AI Pitfalls to Avoid

#### 1. **Hallucinated APIs & Functions**
- **NEVER** suggest non-existent Qt6 or C++ standard library functions
- **ALWAYS** verify API availability before suggesting usage
- **NEVER** assume newer C++ features are available without checking compiler support
- **ALWAYS** check existing codebase for similar implementations before creating new ones

#### 2. **Inappropriate Dependencies**
- **NEVER** suggest adding new dependencies without checking vcpkg.json compatibility
- **NEVER** recommend libraries that conflict with Qt6 or existing dependencies
- **ALWAYS** prefer existing project dependencies over new ones
- **ALWAYS** consider cross-platform compatibility (Windows primary, but portable)

#### 3. **Memory Management Mistakes**
- **NEVER** mix Qt's object ownership model with manual memory management
- **ALWAYS** use Qt smart pointers (QScopedPointer, QSharedPointer) appropriately
- **NEVER** suggest raw pointers for object ownership
- **ALWAYS** consider Qt's parent-child object lifetime management

#### 4. **Threading & Async Pitfalls**
- **NEVER** suggest blocking UI thread with heavy operations
- **ALWAYS** use Qt's threading mechanisms (QThread, QtConcurrent) over std::thread
- **NEVER** access UI elements from non-UI threads without proper signals/slots
- **ALWAYS** consider Qt's event loop and signal/slot mechanism

#### 5. **Build System Confusion**
- **NEVER** suggest modifications to CMake that break existing presets
- **ALWAYS** respect the multi-configuration build setup (Debug/Release)
- **NEVER** hardcode paths that won't work across different environments
- **ALWAYS** use CMake variables and targets properly

#### 6. **Cross-Platform Issues**
- **NEVER** suggest Windows-only solutions without noting platform limitations
- **ALWAYS** consider file path separators and encoding differences
- **NEVER** assume specific compiler behaviors across platforms
- **ALWAYS** use Qt's cross-platform abstractions when available

### Video Processing Specific Guidelines

#### Performance Considerations
- **ALWAYS** consider memory usage for large video files
- **NEVER** load entire videos into memory without streaming consideration
- **ALWAYS** use appropriate data structures for frame-by-frame processing
- **ALWAYS** consider GPU acceleration opportunities where applicable

#### Format Support
- **NEVER** assume codec availability without proper detection
- **ALWAYS** provide fallback mechanisms for unsupported formats
- **NEVER** hardcode format-specific assumptions
- **ALWAYS** validate media file integrity before processing

#### GPU Integration
- **NEVER** suggest Direct3D code that bypasses existing abstraction layers
- **ALWAYS** use the established GPU bridge pattern for hardware acceleration
- **NEVER** assume specific GPU vendor availability
- **ALWAYS** provide CPU fallbacks for GPU operations

### Code Review Mindset

#### Before Suggesting Code Changes
1. **Understand the context** - Read surrounding code and comments
2. **Check existing patterns** - Follow established conventions in the codebase
3. **Consider impact** - Think about performance, maintainability, and compatibility
4. **Verify correctness** - Ensure suggested code actually compiles and works
5. **Security review** - Check for potential vulnerabilities or resource leaks

#### Red Flags to Avoid
- Suggesting code that doesn't match the existing style
- Proposing solutions that ignore established error handling patterns
- Recommending changes that break existing functionality
- Ignoring const-correctness and modern C++ best practices
- Suggesting premature optimizations without profiling data

### Testing & Validation

#### Test-Driven Approach
- **ALWAYS** consider how suggested code can be tested
- **NEVER** suggest code that's inherently difficult to unit test
- **ALWAYS** respect existing test structure and patterns
- **ALWAYS** consider edge cases and error conditions

#### Documentation Standards
- **ALWAYS** provide clear, concise comments for complex algorithms
- **NEVER** state the obvious in comments
- **ALWAYS** document public API interfaces
- **ALWAYS** update relevant documentation when suggesting changes

### Emergency Protocols

#### If Uncertain
- **NEVER** guess at API signatures or behavior
- **ALWAYS** suggest checking documentation or existing code
- **ALWAYS** provide multiple alternatives when possible
- **NEVER** present uncertain information as fact

#### When Asking for Clarification
- **ALWAYS** be specific about what information is needed
- **NEVER** ask vague questions that don't help progress
- **ALWAYS** explain why the clarification is important
- **ALWAYS** suggest reasonable defaults while waiting for clarification

---

**Remember**: The goal is to assist in building a robust, maintainable, and efficient video editor. When in doubt, prefer conservative, well-tested approaches over experimental or cutting-edge solutions.
