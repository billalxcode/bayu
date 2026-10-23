# Files: listing, file items, MIME sniffing, file operations with undo, trash,
# file watching and UDisks2 volumes, on the toolkit's base library alone.
# FilesThumbnails adds the freedesktop thumbnail cache and external
# thumbnailers, which decode through the toolkit's UI library.
#   find_package(BayuFiles REQUIRED)
#   target_link_libraries(app PRIVATE Bayu::Files Bayu::FilesThumbnails)

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(FATAL_ERROR "Bayu Files requires Linux")
endif()

set(CMAKE_AUTOMOC OFF)
set(CMAKE_AUTORCC OFF)

find_package(BayuToolkit REQUIRED)
find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)
# Unicode case folding for name search and MIME glob matching, as the toolkit
# already uses it.
pkg_check_modules(FilesIcu REQUIRED IMPORTED_TARGET icu-uc)
# Reads and writes the thumbnail cache's PNG text chunks.
pkg_check_modules(FilesPng REQUIRED IMPORTED_TARGET libpng)

set(FILES_PUBLIC_HEADERS
    background_release.h
    directory_lister.h
    directory_model.h
    error.h
    file_enumerator.h
    file_item.h
    file_path.h
    file_system_url.h
    file_watcher.h
    local_file_system.h
    mime_types.h
    operation_context.h
    task.h
    trash_monitor.h
    url.h
    volume_manager.h
)

bayu_framework_add_library(bayu-files
    TARGET Files
    MODULE files
    EXPORT BayuFilesTargets
    SOURCES
        async_p.cpp
        async_p.h
        directory_lister.cpp
        directory_model.cpp
        error.cpp
        file_enumerator.cpp
        file_item.cpp
        file_path.cpp
        file_system_url.cpp
        file_watcher.cpp
        local_file_system.cpp
        mime_sniffer.cpp
        mime_sniffer_p.h
        mime_types.cpp
        operation_context.cpp
        path_utils_p.h
        posix_p.h
        task.cpp
        trash.cpp
        trash_directory.cpp
        trash_directory_p.h
        trash_monitor.cpp
        url.cpp
        volume_manager.cpp
    HEADERS ${FILES_PUBLIC_HEADERS}
    PUBLIC_LIBRARIES Bayu::ToolkitBase Threads::Threads
    PRIVATE_LIBRARIES PkgConfig::FilesIcu
)
target_compile_definitions(bayu-files PRIVATE _FILE_OFFSET_BITS=64)

bayu_framework_add_library(bayu-files-thumbnails
    TARGET FilesThumbnails
    MODULE files
    EXPORT BayuFilesTargets
    SOURCES
        thumbnails/md5_p.cpp
        thumbnails/md5_p.h
        thumbnails/thumbnail_cache.cpp
        thumbnails/thumbnailers.cpp
        thumbnails/thumbnailers_p.h
    HEADERS thumbnails/thumbnail_cache.h
    PUBLIC_LIBRARIES Bayu::Files Bayu::Toolkit
    PRIVATE_LIBRARIES PkgConfig::FilesPng
)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

bayu_framework_export(BayuFiles BayuFilesTargets)
bayu_framework_add_package(BayuFiles)
