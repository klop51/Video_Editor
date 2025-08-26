# Centralized compiler warnings configuration
function(setup_project_warnings)
    if(MSVC)
        # High warning level
        add_compile_options(/W4 /permissive-)
        # Stricter conformance checks (MSVC specific)
        add_compile_options(/w14242 /w14254 /w14263 /w14265 /w14287 /we4289 /w14296 /w14311 /w14545 /w14546 /w14547 /w14549 /w14555 /w14640 /w14826 /w14905 /w14906 /w14928)
        if(ENABLE_WARNINGS_AS_ERRORS)
            add_compile_options(/WX)
        endif()
        # Disable selected noisy or external warnings
        add_compile_options(/wd4251 /wd4275 /wd4996)
    else()
        add_compile_options(-Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow -Wdouble-promotion -Wformat=2 -Wundef -Wlogical-op)
        if(ENABLE_WARNINGS_AS_ERRORS)
            add_compile_options(-Werror)
        endif()
    endif()
endfunction()
