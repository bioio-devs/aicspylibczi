
set(UNIT_TEST_SOURCE_LIST test_Reader.cpp test_boundingboxes.cpp test_Iterator.cpp test_Image.cpp test_DimIndex.cpp
        test_main.cpp ../_aicspylibczi/pb_helpers.cpp)
set(TARGET_NAME test_libczi_c++_extension)
set(TARGET_EXE libczi+py)
add_compile_definitions(_STATICLIBBUILD)
set(CMAKE_VERBOSE_MAKEFILE ON)

include_directories(../_aicspylibczi ${CMAKE_BINARY_DIR}/libCZI/Src/libCZI)
add_executable(${TARGET_NAME} ${UNIT_TEST_SOURCE_LIST})
target_link_libraries(${TARGET_NAME} PRIVATE libczi_c++_extension libCZIStatic JxrDecodeStatic nanobind-static Python::Python)
set_target_properties(${TARGET_NAME} PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES)
add_dependencies(${TARGET_NAME} libczi_c++_extension libCZIStatic JxrDecodeStatic )


add_custom_target(build-time-make-directory ${TARGET_NAME}
        COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:${TARGET_NAME}>/resources
        COMMENT "Copying python test resources")
add_custom_command(TARGET ${TARGET_NAME} PRE_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/c_tests/resources $<TARGET_FILE_DIR:${TARGET_NAME}>/resources
        COMMENT "Copying C++ test resources")

if(DEFINED ENV{RUN_TESTS})
    add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
            COMMAND ${TARGET_NAME}
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/c_tests
            COMMENT "Running tests..."
            )
endif()
