option(STATE_SWAPPING "STATE_SWAPPING" OFF)
option(CSR_CONTEXT "CSR_CONTEXT" OFF)



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

set(src-utils
  utils/error.c
)

set(src-lp
  lp/lp.c
  lp/lp_lock.c
  lp/mm/segment/buddy.c
  lp/mm/segment/segment.c
  lp/mm/allocator/dymelor.c
  lp/mm/checkpoints/checkpoints.c
  lp/rollback.c
)

set(src-os
  os/filesystem.c
  os/numa.c
  os/thread.c
)


set(src-core
        core/core.c
        core/x86.c
        core/queue.c
        core/main.c
        core/metrics_for_lp_migration.c
        core/state_swapping.c
        core/virtual_timer.c
        gvt/gvt.c
        configuration/configuration.c
        scheduler/fetch.c
        scheduler/nb_calqueue.c
        scheduler/local/local_scheduler.c
        scheduler/local/metrics_for_window.c
        scheduler/local/local_index/local_index.c
        numerical/numerical.c
        statistics/statistics.c
        gc/garbagecollector.c)

set(src-datatypes
        datatypes/list.c
)

set(src-reverse
        reverse/reverse.c
        reverse/slab.c
)


set(src-api
        api/api.c
        api/topology.c
)



# Build the core library
include_directories(include)
include_directories(include-gen)
include_directories(allocators)
include_directories(core)
include_directories(../mm-nvram/include)
include_directories(../mm-nvram/memkind-install/include)



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

add_library(use-datatypes STATIC ${src-datatypes})
add_library(use-utils     STATIC ${src-utils})
add_library(use-os        STATIC ${src-os})
add_library(use-api       STATIC ${src-api})
add_library(use-lp        STATIC ${src-lp})
#add_library(use-reverse  STATIC ${src-reverse})


add_custom_target(libuse-core-mocked.a
  DEPENDS ${name} use-core use-datatypes use-lp
  COMMAND ld -r --wrap malloc --wrap free --wrap realloc --wrap calloc --wrap aligned_alloc --wrap posix_memalign 
  -o libuse-core-mocked.a --whole-archive 
  ${CMAKE_BINARY_DIR}/src/libuse-core.a 
  ${CMAKE_BINARY_DIR}/src/libuse-datatypes.a
  ${CMAKE_BINARY_DIR}/src/libuse-lp.a 
)


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
