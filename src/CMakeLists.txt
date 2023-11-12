if (APPLE)
    set(OS_SPECIFIC_FILES "create_pipe.macos.cpp")
else ()
    set(OS_SPECIFIC_FILES "create_pipe.linux.cpp")
endif ()

add_executable(manager 
    manager.cpp
    create_pipe.hpp
    ${OS_SPECIFIC_FILES})

target_link_libraries(manager
    PRIVATE
        everest::framework
        Boost::program_options
)

if (EVEREST_ENABLE_ADMIN_PANEL_BACKEND)
    target_link_libraries(manager
        PRIVATE
            controller-ipc
    )

    target_compile_definitions(manager PRIVATE ENABLE_ADMIN_PANEL)

    add_subdirectory(controller)
endif ()

target_compile_options(manager PRIVATE ${COMPILER_WARNING_OPTIONS})

target_compile_features(manager PRIVATE cxx_std_17)

install(
    TARGETS manager
    RUNTIME
)

# FIXME (aw): the www folder currently always needs to exist, so that the manager does not complain
install(
    DIRECTORY # intentionally left blank
    DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/everest/www"
)
