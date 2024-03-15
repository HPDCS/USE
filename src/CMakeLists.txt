option(STATE_SWAPPING "STATE_SWAPPING" OFF)
option(CSR_CONTEXT "CSR_CONTEXT" OFF)
option(BUDDY "BUDDY" OFF)



#nbcalcqueue-related macros
add_compile_options( 
-DEPB=3 
-DPUB=0.333 
-DPRUNE_PERIOD=5000
)

#scheduling-related options
add_compile_options( 
-DOPTIMISTIC_MODE=1
-DMAX_SKIPPED_LP=100000 
)

#log-related options
add_compile_options(
-DREPORT=1 
)

#buddy-page option
if(BUDDY)
add_compile_options(
-DBUDDY=1
)
else()
add_compile_options(
-DBUDDY=0
)
endif()

#reversibility related options
#option(REVERSIBLE "REVERSIBLE" OFF)
#add_compile_options(
#-DREVERSIBLE=0
#)


if(CSR_CONTEXT)
add_compile_options(
-DCSR_CONTEXT=1
)
else()
add_compile_options(
-DCSR_CONTEXT=0
)
endif()

add_compile_options(
-Iidt_patcher/include/
-Isyscall_table_patcher/include/
-Itrap-based-user-context/include/ 
)

if (CMAKE_BUILD_TYPE STREQUAL  "Debug")
    add_compile_options(
    -DDEBUG=1
    -g
    )
endif()

set(src-core
        core/core.c
        core/configuration.c
        core/nb_calqueue.c
        core/x86.c
        core/topology.c
        core/queue.c
        core/fetch.c
        core/main.c
        core/numerical.c
        core/local_scheduler.c
        core/local_index/local_index.c
        core/metrics_for_window.c
        core/metrics_for_lp_migration.c
        core/state_swapping.c
        core/virtual_timer.c
        core/lp_lock.c
        core/gvt.c
        core/incremental_state_saving.c
        core/iss_model.c
        statistics/statistics.c
        mm/garbagecollector.c)

set(src-datatypes
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


add_custom_target(
        clock_constant
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_SOURCE_DIR}/include-gen/
        COMMAND cd ${CMAKE_CURRENT_SOURCE_DIR}/build_scripts && ./gen_clock_header.sh && mv clock_constant.h ../include-gen
)
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} APPEND PROPERTY ADDITIONAL_MAKE_CLEAN_FILES ${PROJECT_SOURCE_DIR}/src/include-gen/clock_constant.h)

add_custom_target(
        cache_db
        COMMAND cd ${CMAKE_CURRENT_SOURCE_DIR}/build_scripts && ./get_cache_data.sh
)
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} APPEND PROPERTY ADDITIONAL_MAKE_CLEAN_FILES ${PROJECT_SOURCE_DIR}/src/build_scripts/cache.db)

add_custom_target(
        hpipe
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_SOURCE_DIR}/include-gen/
        COMMAND cd ${CMAKE_CURRENT_SOURCE_DIR}/build_scripts && python3 build_cache_map.py cache.db > hpipe.h && mv hpipe.h ../include-gen
        DEPENDS cache_db
)
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} APPEND PROPERTY ADDITIONAL_MAKE_CLEAN_FILES ${PROJECT_SOURCE_DIR}/src/include-gen/hpipe.h)

if(CSR_CONTEXT)
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
endif()






add_library(use-core      STATIC ${src-core} )

add_dependencies(use-core clock_constant hpipe)

if(CSR_CONTEXT)
add_dependencies(use-core trap-based-user-context)
endif()

add_library(use-mm        STATIC ${src-mm})
#add_library(use-reverse   STATIC ${src-reverse})
add_library(use-datatypes   STATIC ${src-datatypes})



if(CSR_CONTEXT)
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
endif()