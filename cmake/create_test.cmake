function(ginkgo_create_test test_name)
    file(RELATIVE_PATH REL_BINARY_DIR
         ${PROJECT_BINARY_DIR} ${CMAKE_CURRENT_BINARY_DIR})
    string(REPLACE "/" "_" TEST_TARGET_NAME "${REL_BINARY_DIR}/${test_name}")
    add_executable(${TEST_TARGET_NAME} ${test_name}.cpp)
    target_include_directories("${TEST_TARGET_NAME}"
        PRIVATE
            "$<BUILD_INTERFACE:${Ginkgo_BINARY_DIR}>"
        )
    set_target_properties(${TEST_TARGET_NAME} PROPERTIES
        OUTPUT_NAME ${test_name})
    target_link_libraries(${TEST_TARGET_NAME} PRIVATE ginkgo GTest::Main GTest::GTest ${ARGN})
    add_test(NAME ${REL_BINARY_DIR}/${test_name} COMMAND ${TEST_TARGET_NAME})
endfunction(ginkgo_create_test)

function(ginkgo_create_cuda_test test_name)
    file(RELATIVE_PATH REL_BINARY_DIR
         ${PROJECT_BINARY_DIR} ${CMAKE_CURRENT_BINARY_DIR})
    string(REPLACE "/" "_" TEST_TARGET_NAME "${REL_BINARY_DIR}/${test_name}")
    add_executable(${TEST_TARGET_NAME} ${test_name}.cu)
    target_include_directories("${TEST_TARGET_NAME}"
        PRIVATE
            "$<BUILD_INTERFACE:${Ginkgo_BINARY_DIR}>"
        )
    set_target_properties(${TEST_TARGET_NAME} PROPERTIES
        OUTPUT_NAME ${test_name})
    target_link_libraries(${TEST_TARGET_NAME} PRIVATE ginkgo GTest::Main GTest::GTest ${ARGN})
    add_test(NAME ${REL_BINARY_DIR}/${test_name} COMMAND ${TEST_TARGET_NAME})
endfunction(ginkgo_create_cuda_test)

function(ginkgo_create_hip_test test_name)
    set (CMAKE_LINKER "${HIP_PATH}/bin/hipcc")
    set (CMAKE_CXX_LINK_EXECUTABLE "<CMAKE_LINKER> <FLAGS> <CMAKE_CXX_LINK_FLAGS> <LINK_FLAGS> <OBJECTS> -o <TARGET> <LINK_LIBRARIES>")

    file(RELATIVE_PATH REL_BINARY_DIR
         ${PROJECT_BINARY_DIR} ${CMAKE_CURRENT_BINARY_DIR})
    string(REPLACE "/" "_" TEST_TARGET_NAME "${REL_BINARY_DIR}/${test_name}")
    set(CMAKE_CXX_LINK_EXECUTABLE ${CMAKE_CXX_COMPILER})
    set_source_files_properties(${test_name}.hip.cpp PROPERTIES HIP_SOURCE_PROPERTY_FORMAT TRUE)
    hip_add_executable(${TEST_TARGET_NAME} ${test_name}.hip.cpp)
    target_include_directories("${TEST_TARGET_NAME}"
        PRIVATE
            "$<BUILD_INTERFACE:${Ginkgo_BINARY_DIR}>"
        )
    set_target_properties(${TEST_TARGET_NAME} PROPERTIES
        OUTPUT_NAME ${test_name})
    if(GINKGO_HIP_AMDGPU AND GINKGO_HIP_PLATFORM STREQUAL "hcc")
        foreach(target ${GINKGO_HIP_AMDGPU})
            target_link_libraries(${TEST_TARGET_NAME} PRIVATE --amdgpu-target=${target})
        endforeach()
    endif()

    target_link_libraries(${TEST_TARGET_NAME} PRIVATE ginkgo GTest::Main GTest::GTest  ${ARGN})

    # GINKGO_RPATH_FOR_HIP needs to be populated before calling this for the linker to include
    # our libraries path into the executable's runpath.
    if(BUILD_SHARED_LIBS)
        if (GINKGO_HIP_PLATFORM MATCHES "hcc")
            target_link_libraries(${TEST_TARGET_NAME} PRIVATE "${GINKGO_RPATH_FOR_HIP}")
        elseif(GINKGO_HIP_PLATFORM MATCHES "nvcc")
            target_link_libraries(${TEST_TARGET_NAME} PRIVATE -Xcompiler \"${GINKGO_RPATH_FOR_HIP}\")
        endif()
    endif()
    add_test(NAME ${REL_BINARY_DIR}/${test_name} COMMAND ${TEST_TARGET_NAME})
endfunction(ginkgo_create_hip_test)
