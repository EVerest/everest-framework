
set(EVEREST_VERSION_INFORMATION_HEADER_IN "${CMAKE_CURRENT_LIST_DIR}/assets/version_information.hpp.in")

function (evc_generate_version_information)
    execute_process(
        COMMAND
            git describe --dirty --always --tags
        WORKING_DIRECTORY
            ${PROJECT_SOURCE_DIR}
        OUTPUT_VARIABLE
            GIT_VERSION
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    # make version information available as c++ header
    configure_file("${EVEREST_VERSION_INFORMATION_HEADER_IN}" "${CMAKE_CURRENT_BINARY_DIR}/generated/include/generated/version_information.hpp" @ONLY)
endfunction()
