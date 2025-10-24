# Get the current commit
execute_process(
    COMMAND git describe --always --dirty
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_COMMIT
    OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE GIT_COMMIT_RESULT
)

if(NOT GIT_COMMIT_RESULT EQUAL 0)
  set(GIT_COMMIT "unknown")
endif()

# Make it available as a define to the compiler
add_compile_definitions(AX08_IO_FW_GIT_COMMIT=${GIT_COMMIT})
