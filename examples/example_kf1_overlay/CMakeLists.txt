project(example_kf1_overlay)

add_executable(${PROJECT_NAME} example_kf1_overlay.cpp)
set_target_properties(
    ${PROJECT_NAME}
    PROPERTIES CXX_STANDARD 23
               ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/out/${CMAKE_BUILD_TYPE}"
               LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/out/${CMAKE_BUILD_TYPE}"
               RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/out/${CMAKE_BUILD_TYPE}")

# kf1_common/kf1_target.hpp is shared with example_kf1_dumper
target_include_directories(${PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/..")

find_package(OpenGL REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE omath::omath glfw imgui::imgui OpenGL::GL)

# The same overlay as a DirectX 9 hook that lives inside the game. KillingFloor.exe is a 32 bit process, a 64 bit DLL
# could not even be loaded into it, so this only builds for a 32 bit target (windows-*-vcpkg-x86 presets)
if(OMATH_ENABLE_HOOKING AND CMAKE_SIZEOF_VOID_P EQUAL 4)
    add_library(${PROJECT_NAME}_dll MODULE dllmain.cpp)
    set_target_properties(
        ${PROJECT_NAME}_dll
        PROPERTIES CXX_STANDARD 23
                   MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
                   ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/out/${CMAKE_BUILD_TYPE}"
                   LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/out/${CMAKE_BUILD_TYPE}"
                   RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/out/${CMAKE_BUILD_TYPE}")

    # The menu's look. It is a checkout of its own next to this repository, not a dependency of omath
    set(KF1_IMGUI_NULLCORE_DIR "${CMAKE_SOURCE_DIR}/../imgui-nullcore" CACHE PATH "imgui-nullcore checkout")
    add_subdirectory("${KF1_IMGUI_NULLCORE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/imgui-nullcore")
    set_target_properties(imgui_nullcore PROPERTIES MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

    # The task bar's start button and the start menu's icons only exist as the preview's generated data
    target_include_directories(${PROJECT_NAME}_dll PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/.."
                                                           "${KF1_IMGUI_NULLCORE_DIR}/preview/data")
    target_link_libraries(${PROJECT_NAME}_dll PRIVATE omath::omath imgui::imgui imgui_nullcore::imgui_nullcore d3d9)
endif()
