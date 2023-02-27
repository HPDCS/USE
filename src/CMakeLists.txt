option(STATE_SWAPPING "STATE_SWAPPING" ON)
option(CSR_CONTEXT "CSR_CONTEXT" OFF)



#nbcalcqueue-related macros
add_compile_options( 
-DEPB=3 
-DPUB=0.333 
-DPRUNE_PERIOD=500
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
-DCHECKPOINT_PERIOD=10
-DCLEAN_CKP_INTERVAL=100
)

#ongvt-related options
if(STATE_SWAPPING)
add_compile_options(
-DONGVT_PERIOD=-1
)
else()
add_compile_options(
-DONGVT_PERIOD=500
)
endif()

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

if(STATE_SWAPPING)
add_compile_options(
-DSTATE_SWAPPING=1
)
else() 
add_compile_options(
-DSTATE_SWAPPING=0
)
endif()

if(CSR_CONTEXT)
add_compile_options(
-DCSR_CONTEXT=1
)
else()
add_compile_options(
-DCSR_CONTEXT=0
)
endif()

if(STATE_SWAPPING AND CSR_CONTEXT)
add_compile_options(
-DSTATE_SWAPPING=1
-DCSR_CONTEXT=1
)

add_compile_options(
-Iidt_patcher/include/
-Isyscall_table_patcher/include/
-Itrap-based-user-context/include/ 
)

endif()

if (CMAKE_BUILD_TYPE STREQUAL  "Debug")
    add_compile_options(
    -DDEBUG=1
    -g
    )
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
        core/virtual_timer.c
        core/lp_lock.c
        core/gvt.c
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
        OUTPUT ${PROJECT_SOURCE_DIR}/include-gen/clock_constant.h
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

add_custom_command(
        OUTPUT kallsyms_lookup_name_exporter
        COMMAND git clone git@github.com:HPDCS/kallsyms_lookup_name_exporter.git || echo already downloaded
        COMMAND cd kallsyms_lookup_name_exporter && make clean && make && cd ..
)

add_custom_command(
        OUTPUT idt_patcher
        COMMAND git clone git@github.com:HPDCS/idt_patcher.git || echo already downloaded
        COMMAND cd idt_patcher && make clean && make && cd ..
)

add_custom_command(
        OUTPUT syscall_table_patcher
        COMMAND git clone git@github.com:HPDCS/syscall_table_patcher.git || echo already downloaded
        COMMAND cd syscall_table_patcher && make clean && make && cd ..
)

add_custom_command(
        OUTPUT trap-based-user-context
        COMMAND git clone git@github.com:HPDCS/trap-based-user-context.git || echo already downloaded
        COMMAND cd trap-based-user-context && make clean && ./compile.sh && cd ..
        DEPENDS kallsyms_lookup_name_exporter idt_patcher syscall_table_patcher
)


add_library(use-core      STATIC 
${src-core} 
${CMAKE_CURRENT_SOURCE_DIR}/include-gen/clock_constant.h 
${CMAKE_CURRENT_SOURCE_DIR}/include-gen/hpipe.h
trap-based-user-context
)



add_library(use-mm        STATIC ${src-mm})
#add_library(use-reverse   STATIC ${src-reverse})
add_library(use-datatypes   STATIC ${src-datatypes})



add_custom_target(
        remove_modules
        COMMAND sudo rmmod trap-based-usercontext
        COMMAND sudo rmmod syscall_table_patcher           
        COMMAND sudo rmmod kln_exporter           
        COMMAND sudo rmmod idt_patcher           
)


add_custom_target(
        update_modules
        COMMAND cd idt_patcher                    && git pull  && make clean && make && cd ..
        COMMAND cd syscall_table_patcher          && git pull  && make clean && make && cd ..
        COMMAND cd kallsyms_lookup_name_exporter  && git pull  && make clean && make && cd ..
        COMMAND cd trap-based-user-context        && git pull  && make clean && ./compile.sh && cd ..
)

add_custom_target(
        mount_modules
        COMMAND cd idt_patcher                    && ./mount.sh && cd ..
        COMMAND cd syscall_table_patcher          && ./mount.sh && cd ..
        COMMAND cd kallsyms_lookup_name_exporter  && ( sudo rmmod kln_exporter            || sudo insmod kln_exporter.ko           ) && cd ..
        COMMAND cd trap-based-user-context        && ( sudo rmmod trap-based-user-context || sudo insmod trap-based-usercontext.ko ) && cd ..
)


#add_custom_command(
#        TARGET use-core  POST_BUILD
#        COMMAND ${CMAKE_COMMAND} -E copy
#                ${CMAKE_CURRENT_SOURCE_DIR}/include/ROOT-Sim.h
#                ${CMAKE_CURRENT_BINARY_DIR})
#
#add_custom_command(
#        TARGET use-core  POST_BUILD
#        COMMAND ${CMAKE_COMMAND} -E copy
#                ${CMAKE_CURRENT_SOURCE_DIR}/include-gen/clock_constant.h
#                ${CMAKE_CURRENT_BINARY_DIR})
#
#add_custom_command(
#        TARGET use-core  POST_BUILD
#        COMMAND ${CMAKE_COMMAND} -E copy
#                ${CMAKE_CURRENT_SOURCE_DIR}/include/timer.h
#                ${CMAKE_CURRENT_BINARY_DIR})

