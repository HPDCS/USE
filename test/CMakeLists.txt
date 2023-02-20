function(test_program name)
    add_executable(test_${name} ${ARGN})
    target_include_directories(test_${name} PRIVATE ../src/reverse ../src/include ../src/datatypes ../src/core ../src/statistics ../src/mm ../src)
    add_test(NAME test_${name} COMMAND test_${name})
    target_link_libraries(test_${name} use-datatypes) 
    set_tests_properties(test_${name} PROPERTIES TIMEOUT 120)
endfunction()


function(test_model name cores lps)
    add_library(${name} STATIC ${ARGN})
    #add_dependencies(${name} ${PROJECT_SOURCE_DIR}/src/include-gen/clock_constant.h)
    target_include_directories(${name} PRIVATE ../src/include ../src/include-gen ${name})
    
    add_executable(test_${name} integration/empty.c)
        
    add_custom_target(lib${name}-mm.a
        DEPENDS ${name} use-mm
        COMMAND ld -r --wrap malloc --wrap free --wrap realloc --wrap calloc -o lib${name}-mm.a --whole-archive lib${name}.a ${CMAKE_BINARY_DIR}/src/libuse-mm.a
    )

    add_dependencies(test_${name} ${name} use-mm use-core use-datatypes lib${name}-mm.a)
    
    target_link_libraries(test_${name} ${CMAKE_BINARY_DIR}/test/lib${name}-mm.a use-core use-datatypes m numa pthread) 

    add_test(NAME test_${name}_seq COMMAND test_${name} 1 ${lps})
    add_test(NAME test_${name}_par COMMAND test_${name} ${cores} ${lps})
    set_tests_properties(test_${name}_seq PROPERTIES TIMEOUT 120)
    set_tests_properties(test_${name}_par PROPERTIES TIMEOUT 120)
endfunction()


test_program(bitmap datatypes/bitmap.c)
test_program(list datatypes/list.c)

test_model(phold ${N} 1024 ../models/phold/application.c)
target_compile_options(phold PRIVATE -DLOOP_COUNT=1 -DCOMPLETE_EVENTS=10000)


test_model(phold_count ${N} 32 ../models/phold_count/application.c)
target_compile_options(phold_count PRIVATE -DLOOP_COUNT=1 -DCOMPLETE_EVENTS=100000)


test_model(pcs ${N} 1024 ../models/pcs/application.c ../models/pcs/functions_app.c)
target_compile_options(pcs PRIVATE -DCOMPLETE_CALLS=1000 -DTA_CHANGE=300 -DTA_DURATION=120 -DTA=0.12 -DCHANNELS_PER_CELL=8000 -DFADING_RECHECK_FREQUENCY=30)