include(ev-cli)
require_ev_cli_version("0.0.20")

add_custom_target(convert_yaml_refs_schemas)
add_custom_target(convert_yaml_refs_schemas_installable
    ALL
)
#
# convert relative references in yaml files to absolute references
#
function(ev_convert_refs)
    if (ARGC EQUAL 3)
        set (CONVERT_REFS_INPUT_FILE ${ARGV0})
        set (CONVERT_REFS_OUTPUT_DIR ${ARGV1})
        set (CONVERT_REFS_INSTALL_DIR ${ARGV2})
    else()
        message(FATAL_ERROR "${CMAKE_CURRENT_FUNCTION}() expects exactly three arguments")
    endif()

    get_filename_component(CONVERT_REFS_INPUT_FILE_NAME ${CONVERT_REFS_INPUT_FILE} NAME)

    get_filename_component(CONVERT_REFS_INPUT_FILE_NAME_WITHOUT_EXT ${CONVERT_REFS_INPUT_FILE} NAME_WE)
    get_filename_component(CONVERT_REFS_INPUT_FILE_PARENT ${CONVERT_REFS_INPUT_FILE} DIRECTORY)
    cmake_path(GET CONVERT_REFS_INPUT_FILE_PARENT STEM CONVERT_REFS_INPUT_FILE_PARENT_NAME)
    set(CONVERT_REFS_TARGET_NAME "convert_yaml_refs_${CONVERT_REFS_INPUT_FILE_PARENT_NAME}_${CONVERT_REFS_INPUT_FILE_NAME_WITHOUT_EXT}")

    set(CONVERT_REFS_OUTPUT_FILE "${CONVERT_REFS_OUTPUT_DIR}/${CONVERT_REFS_INPUT_FILE_NAME}")
    add_custom_command(
        OUTPUT
            "${CONVERT_REFS_OUTPUT_FILE}"
        DEPENDS
            ${CONVERT_REFS_INPUT_FILE}
        COMMENT
            "Converting relative references in ${CONVERT_REFS_INPUT_FILE_NAME} to absolute references..."
        VERBATIM
        COMMAND
            ${EV_CLI} helpers convert-refs
                "${CONVERT_REFS_INPUT_FILE}"
                "${CONVERT_REFS_OUTPUT_FILE}"
        WORKING_DIRECTORY
            ${PROJECT_SOURCE_DIR}
    )
    add_custom_target( ${CONVERT_REFS_TARGET_NAME}
        DEPENDS 
            "${CONVERT_REFS_OUTPUT_FILE}"
    )
    add_dependencies( convert_yaml_refs_schemas
        "${CONVERT_REFS_TARGET_NAME}"
    )

    # Install target
    set(CONVERT_REFS_BASE_PATH "${CMAKE_INSTALL_PREFIX}/${CONVERT_REFS_INSTALL_DIR}/${CONVERT_REFS_INPUT_FILE_NAME}")

    set(CONVERT_REFS_OUTPUT_FILE_INSTALLABLE "${CONVERT_REFS_OUTPUT_DIR}_installable/${CONVERT_REFS_INPUT_FILE_NAME}")
    add_custom_command(
        OUTPUT
            "${CONVERT_REFS_OUTPUT_FILE_INSTALLABLE}"
        DEPENDS
            ${CONVERT_REFS_INPUT_FILE}
        COMMENT
            "Converting relative references in ${CONVERT_REFS_INPUT_FILE_NAME} to absolute references..."
        VERBATIM
        COMMAND
            ${EV_CLI} helpers convert-refs
                "${CONVERT_REFS_INPUT_FILE}"
                "${CONVERT_REFS_OUTPUT_FILE_INSTALLABLE}"
                --base-path "${CONVERT_REFS_BASE_PATH}"
        WORKING_DIRECTORY
            ${PROJECT_SOURCE_DIR}
    )
    add_custom_target( ${CONVERT_REFS_TARGET_NAME}_installable        
        ALL
        DEPENDS 
            "${CONVERT_REFS_OUTPUT_FILE_INSTALLABLE}"
    )
    add_dependencies( convert_yaml_refs_schemas_installable
        "${CONVERT_REFS_TARGET_NAME}_installable"
    )
    install(
        FILES
            "${CONVERT_REFS_OUTPUT_FILE_INSTALLABLE}"
        DESTINATION
            "${CONVERT_REFS_INSTALL_DIR}"
    )
endfunction()
