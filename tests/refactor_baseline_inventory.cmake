if(NOT DEFINED IMAGEVIEWPORT_SOURCE_DIR)
    message(FATAL_ERROR "IMAGEVIEWPORT_SOURCE_DIR is required")
endif()

if(NOT DEFINED IMAGEVIEWPORT_BUILD_DIR)
    set(IMAGEVIEWPORT_BUILD_DIR "${CMAKE_BINARY_DIR}")
endif()

find_program(GIT_EXECUTABLE git)
if(NOT GIT_EXECUTABLE)
    message(WARNING "git not found; skipping refactor baseline inventory")
    return()
endif()

file(MAKE_DIRECTORY "${IMAGEVIEWPORT_BUILD_DIR}/tests")

# Write a git-grep inventory used to compare controller refactor boundaries.
function(write_inventory inventory_name pattern output_file)
    execute_process(
        COMMAND
            "${GIT_EXECUTABLE}" -C "${IMAGEVIEWPORT_SOURCE_DIR}" grep -n -E "${pattern}" --
            src tests
        RESULT_VARIABLE grep_result
        OUTPUT_VARIABLE grep_output
        ERROR_VARIABLE grep_error)

    if(grep_result GREATER 1)
        message(WARNING "Inventory ${inventory_name} failed: ${grep_error}")
        set(grep_output "")
    endif()

    string(STRIP "${grep_output}" stripped_output)
    if(stripped_output STREQUAL "")
        set(entry_count 0)
        set(file_content "No matches.\n")
    else()
        string(REGEX MATCHALL "\n" newline_matches "${stripped_output}")
        list(LENGTH newline_matches newline_count)
        math(EXPR entry_count "${newline_count} + 1")
        set(file_content "${stripped_output}\n")
    endif()

    file(WRITE "${output_file}" "${file_content}")
    message(STATUS "${inventory_name}: ${entry_count} entries written to ${output_file}")
endfunction()

write_inventory(
    "role-specific state and branch inventory"
    "(primary[A-Za-z0-9_]*|secondary[A-Za-z0-9_]*|PageRole::Primary|PageRole::Secondary|role[ \t]*==[ \t]*ImageViewport::PageRole::(Primary|Secondary))"
    "${IMAGEVIEWPORT_BUILD_DIR}/tests/refactor-role-inventory.txt")

write_inventory(
    "command diagnostic mutation inventory"
    "(setCommandDiagnostic|clearCommandDiagnosticForAcceptedCommand|commandRevision|CommandReason)"
    "${IMAGEVIEWPORT_BUILD_DIR}/tests/refactor-command-diagnostic-inventory.txt")
