install(DIRECTORY framework/ DESTINATION include/everest
        FILES_MATCHING PATTERN "*.hpp"
)

install(FILES utils/thread.hpp
        DESTINATION include/everest/utils
)
