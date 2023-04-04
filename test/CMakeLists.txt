function(test_program name)
    add_executable(test_${name} ${ARGN})
    target_include_directories(test_${name} PRIVATE ../src/reverse ../src/include ../src/datatypes ../src/core ../src/statistics ../src/mm ../src)
    add_test(NAME test_${name} COMMAND test_${name})
    target_link_libraries(test_${name} use-datatypes) 
    set_tests_properties(test_${name} PROPERTIES TIMEOUT 120)
endfunction()


function(test_model name)
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

endfunction()

function(configure_test name)
    add_test(NAME test_${name}_seq COMMAND test_${name} --ncores=1 ${ARGN})
    add_test(NAME test_${name}_par COMMAND test_${name} --ncores=${N} ${ARGN})
    set_tests_properties(test_${name}_seq PROPERTIES TIMEOUT 120)
    set_tests_properties(test_${name}_par PROPERTIES TIMEOUT 120)
endfunction()

test_program(bitmap datatypes/bitmap.c)
test_program(list datatypes/list.c)

test_model(phold ../models/phold/application.c)
test_model(phold_count ../models/phold_count/application.c)
test_model(pcs ../models/pcs/application.c ../models/pcs/functions_app.c)


configure_test(phold_count --nprocesses=256 --num-events=500 --ongvt-evt-period=50)
configure_test(phold --nprocesses=256 --event-us=5 --num-events=1000)
configure_test(pcs --nprocesses=256 --ta=0.48 --num-calls=500)