cmake_minimum_required(VERSION 3.16)

project(MIA_2S2026_P1_202405047)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)

# Crow
FetchContent_Declare(
    Crow
    GIT_REPOSITORY https://github.com/CrowCpp/Crow.git
    GIT_TAG v1.2.1
)

FetchContent_MakeAvailable(Crow)


#backend
add_executable(
    202405047_ext2_backend

    src/main.cpp

    #analysis
    src/analysis/CommandToken.cpp
    src/analysis/CommandParser.cpp

    #commands
    src/commands/MkDiskCommand.cpp
    src/commands/RmDiskCommand.cpp
    src/commands/FDiskCommand.cpp
    src/commands/MountCommand.cpp
    src/commands/MountedCommand.cpp
    src/commands/MkFsCommand.cpp
    src/commands/CatCommand.cpp
    src/commands/LoginCommand.cpp
    src/commands/LogoutCommand.cpp
    src/commands/MkGrpCommand.cpp
    src/commands/RmGrpCommand.cpp
    src/commands/MkUsrCommand.cpp
    src/commands/RmUsrCommand.cpp
    src/commands/ChGrpCommand.cpp
    src/commands/MkFileCommand.cpp
    src/commands/MkDirCommand.cpp
    src/commands/RepCommand.cpp

    #managers
    src/managers/DiskManager.cpp
    src/managers/MountManager.cpp
    src/managers/FileSystemManager.cpp
    src/managers/UserManager.cpp
    src/managers/FileManager.cpp
    src/managers/ReportManager.cpp

    #utils
    src/utils/BinaryUtils.cpp
    src/utils/PathUtils.cpp
    src/utils/StringUtils.cpp
)


target_include_directories(
    202405047_ext2_backend
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)


target_link_libraries(
    202405047_ext2_backend
    PRIVATE
    Crow::Crow
)