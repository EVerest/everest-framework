include(convert-refs)
set(SCHEMA_FILES
    interface.yaml
    config.yaml
    manifest.yaml
    type.yaml
    error-declaration-list.yaml
    error-object.yaml
)
foreach(SCHEMA ${SCHEMA_FILES})
    ev_convert_refs(
        "${CMAKE_CURRENT_SOURCE_DIR}/${SCHEMA}"
        "${CMAKE_BINARY_DIR}/converted_yamls/schemas"
        "${CMAKE_INSTALL_DATADIR}/everest/schemas"
    )
endforeach()
