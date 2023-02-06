#hardware-related macros
add_compile_options(
-DCACHE_LINE_SIZE=${CACHE_LINE_SIZE} 
-DN_CPU=${N} 
-DARCH_X86_64
)

#nbcalcqueue-related macros
add_compile_options( 
-DEPB=3 
-DPUB=0.333 
-DPRUNE_PERIOD=50
)

#scheduling-related options
add_compile_options( 
-DMAX_SKIPPED_LP=100000 
-DENFORCE_LOCALITY=0 
-DENABLE_DYNAMIC_SIZING_FOR_LOC_ENF=0
-DSTART_WINDOW=0.4 
)

#checkpoint related options
add_compile_options(
-DCHECKPOINT_PERIOD=20 
)

#ongvt-related options
add_compile_options(
-DONGVT_PERIOD=50
)

#log-related options
add_compile_options(
-DREPORT=1 
)

#reversibility related options
#option(REVERSIBLE "REVERSIBLE" OFF)
#add_compile_options(
#-DREVERSIBLE=0
#)

#state-related options
add_compile_options(
-DHAVE_PARALLEL_ALLOCATOR=1 
)

#state swapping options
add_compile_options(
-DSTATE_SWAPPING=1 
)


if (CMAKE_BUILD_TYPE STREQUAL  "Debug")
    add_compile_options(-DDEBUG=1)
endif()

set(src-core
        core/core.c
        core/nb_calqueue.c
        core/x86.c
        core/topology.c
        core/queue.c
        core/fetch.c
        core/main.c
        core/numerical.c
        #core/hpdcs_math.c
        core/parseparam.c
        core/local_scheduler.c
        core/local_index/local_index.c
        core/metrics_for_window.c
        core/metrics_for_lp_migration.c
        core/state_swapping.c
        statistics/statistics.c
        mm/garbagecollector.c)

set(src-datatypes
        #core/calqueue.c
        datatypes/list.c
)

set(src-mm
        mm/buddy.c
        mm/segment.c
        mm/dymelor.c
        mm/recoverable.c
        mm/checkpoints.c
        mm/state.c
        mm/platform.c
)

set(src-reverse
        reverse/reverse.c
        reverse/slab.c
)



# Build the core library
include_directories(include)
include_directories(include-gen)
include_directories(mm)
include_directories(reverse)
include_directories(statistics)
include_directories(datatypes)
include_directories(core)
include_directories(asm)

add_custom_command(
        OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/include-gen/clock_constant.h
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_SOURCE_DIR}/include-gen/
        COMMAND cd ${CMAKE_CURRENT_SOURCE_DIR}/build_scripts && ./gen_clock_header.sh && mv clock_constant.h ../include-gen
)

add_custom_command(
        OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/build_scripts/cache.db
        COMMAND cd ${CMAKE_CURRENT_SOURCE_DIR}/build_scripts && ./get_cache_data.sh
)

add_custom_command(
        OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/include-gen/hpipe.h
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_SOURCE_DIR}/include-gen/
        COMMAND cd ${CMAKE_CURRENT_SOURCE_DIR}/build_scripts && python3 build_cache_map.py cache.db > hpipe.h && mv hpipe.h ../include-gen
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/build_scripts/cache.db
)


add_library(use-core       STATIC ${src-core} ${CMAKE_CURRENT_SOURCE_DIR}/include-gen/clock_constant.h ${CMAKE_CURRENT_SOURCE_DIR}/include-gen/hpipe.h)
add_library(use-mm        STATIC ${src-mm})
#add_library(use-reverse   STATIC ${src-reverse})
add_library(use-datatypes   STATIC ${src-datatypes})


add_custom_command(
        TARGET use-core  POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
                ${CMAKE_CURRENT_SOURCE_DIR}/include/ROOT-Sim.h
                ${CMAKE_CURRENT_BINARY_DIR})

add_custom_command(
        TARGET use-core  POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
                ${CMAKE_CURRENT_SOURCE_DIR}/include-gen/clock_constant.h
                ${CMAKE_CURRENT_BINARY_DIR})

add_custom_command(
        TARGET use-core  POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
                ${CMAKE_CURRENT_SOURCE_DIR}/include/timer.h
                ${CMAKE_CURRENT_BINARY_DIR})

