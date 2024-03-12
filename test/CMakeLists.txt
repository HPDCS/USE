function(test_program name)
    add_executable(test_${name} ${ARGN})
    target_include_directories(test_${name} PRIVATE ../src/reverse ../src/include ../src/datatypes ../src/core ../src/statistics ../src/mm ../src)
    add_test(NAME test_${name} COMMAND test_${name})
    target_link_libraries(test_${name} use-datatypes m) 
    set_tests_properties(test_${name} PROPERTIES TIMEOUT 120)
endfunction()


function(test_model name)
    add_library(${name} STATIC ${ARGN})
    target_include_directories(${name} PRIVATE ../src/include ../src/include-gen ${name})
    
    add_executable(test_${name} integration/empty.c)
        
    add_custom_target(lib${name}-mm.a
        DEPENDS ${name} use-mm
        COMMAND ld -r --wrap malloc --wrap free --wrap realloc --wrap calloc -o lib${name}-mm.a --whole-archive lib${name}.a ${CMAKE_BINARY_DIR}/src/libuse-mm.a
    )

    add_dependencies(test_${name} ${name} use-mm use-core use-datatypes lib${name}-mm.a)
    
    target_link_libraries(test_${name} ${CMAKE_BINARY_DIR}/test/lib${name}-mm.a use-core use-datatypes m numa pthread) 

endfunction()

function(configure_test prefix name)
    add_test(NAME ${prefix}_${name}_seq COMMAND test_${name} --ncores=1 ${ARGN})
    add_test(NAME ${prefix}_${name}_par COMMAND test_${name} --ncores=${N} ${ARGN})
    set_tests_properties(${prefix}_${name}_seq PROPERTIES TIMEOUT 120)
    set_tests_properties(${prefix}_${name}_par PROPERTIES TIMEOUT 120)
endfunction()

test_program(bitmap datatypes/bitmap.c)
test_program(list datatypes/list.c)
test_program(numerical core/numerical.c)
test_program(iss mm/iss.c ../src/core/incremental_state_saving.c ../src/core/iss_model.c)


test_model(phold ../models/phold/application.c)
test_model(phold_count ../models/phold_count/application.c)
test_model(pcs ../models/pcs/application.c ../models/pcs/functions_app.c)
test_model(tuberculosis ../models/tuberculosis/application.c ../models/tuberculosis/guy.c ../models/tuberculosis/guy_init.c ../models/tuberculosis/guy_list.c ../models/tuberculosis/parameters.c)

add_dependencies(phold clock_constant)
add_dependencies(phold_count clock_constant)



configure_test(test phold_count --nprocesses=256 --num-events=500 --ongvt-mode=1 --ongvt-period=50)
configure_test(test phold --nprocesses=256 --event-us=5 --num-events=1000)
configure_test(test pcs --nprocesses=256 --ta=0.48 --num-calls=500 --enable-fading)
configure_test(test_mem pcs --nprocesses=256 --ta=0.48 --num-calls=500 --enable-custom-alloc)
configure_test(test_enf_loc pcs --nprocesses=256 --ta=0.48 --num-calls=1500 --enforce-locality --el-dyn-window --el-locked-size=2 --el-evicted-size=2)
configure_test(test_sync_csr pcs --nprocesses=256 --ta=0.48 --num-calls=500 --ongvt-mode=2 --ongvt-period=500)
configure_test(test_tuberculosis_enf_loc tuberculosis --nprocesses=256 --enforce-locality --el-locked-size=2 --el-evicted-size=2)
