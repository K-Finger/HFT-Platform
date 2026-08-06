# Release settings shared by every compiled target: link-time optimisation and
# native tuning, so the hot path compiles for the machine that builds it.

function(hft_apply_target_options target)
    if(HFT_IPO_SUPPORTED)
        set_target_properties(${target} PROPERTIES INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
    endif()

    if(MSVC)
        target_compile_options(${target} PRIVATE
            $<$<CONFIG:Release>:/O2 /Ob2 /Oi /Ot /arch:AVX2>
        )
    else()
        target_compile_options(${target} PRIVATE
            $<$<CONFIG:Release>:-O3 -march=native>
        )
    endif()
endfunction()
