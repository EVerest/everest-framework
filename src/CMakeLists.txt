add_executable(manager)

IF (APPLE) 
    set (SYSTEM_UNIX_CPP system_unix.macos.cpp)
    set (SYSTEM_UNIX_LIBS "")
ELSE()
    set (SYSTEM_UNIX_CPP system_unix.cpp)
    set (SYSTEM_UNIX_LIBS PkgConfig::libcap)
ENDIF()

target_sources(manager
    PRIVATE
        ${SYSTEM_UNIX_CPP}
        manager.cpp
)

target_link_libraries(manager
    PRIVATE
        everest::framework
        Boost::program_options
        ${SYSTEM_UNIX_LIBS}
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
