cmake_minimum_required(VERSION 3.21)

project(CloganGUI LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC OFF)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(CHIRON_GUI_ENABLE_WINDEPLOYQT "Deploy the Qt runtime into the GUI build directory after linking" OFF)
option(CHIRON_GUI_ENABLE_QT_RUNTIME_DEPLOYMENT "Generate a Qt runtime deployment script for wrapper-driven deployment" OFF)
option(CHIRON_GUI_BUILD_BENCHMARKS "Build CloganGUI performance benchmarks" ON)
option(CHIRON_GUI_BUILD_TESTS "Build CloganGUI unit tests" ON)
set(CHIRON_GUI_OUTPUT_DIRECTORY "" CACHE PATH "Optional staging directory for built executables and libraries")
set(CHIRON_GUI_DEPLOY_SCRIPT "" CACHE INTERNAL "Generated deployment script for wrapper-driven Qt runtime deployment")
set(CHIRON_GUI_THIRD_PARTY_LICENSE_FILE
    "${CMAKE_CURRENT_SOURCE_DIR}/THIRD_PARTY_LICENSES.Qt-Community.txt")

function(chiron_gui_stage_target target_name)
    if(NOT CHIRON_GUI_OUTPUT_DIRECTORY OR NOT TARGET "${target_name}")
        return()
    endif()

    set_target_properties("${target_name}" PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CHIRON_GUI_OUTPUT_DIRECTORY}"
        LIBRARY_OUTPUT_DIRECTORY "${CHIRON_GUI_OUTPUT_DIRECTORY}"
        ARCHIVE_OUTPUT_DIRECTORY "${CHIRON_GUI_OUTPUT_DIRECTORY}"
    )

    foreach(config_name Debug Release RelWithDebInfo MinSizeRel)
        string(TOUPPER "${config_name}" config_name_upper)
        set_target_properties("${target_name}" PROPERTIES
            "RUNTIME_OUTPUT_DIRECTORY_${config_name_upper}" "${CHIRON_GUI_OUTPUT_DIRECTORY}"
            "LIBRARY_OUTPUT_DIRECTORY_${config_name_upper}" "${CHIRON_GUI_OUTPUT_DIRECTORY}"
            "ARCHIVE_OUTPUT_DIRECTORY_${config_name_upper}" "${CHIRON_GUI_OUTPUT_DIRECTORY}"
        )
    endforeach()
endfunction()

find_package(Qt6 6.5 COMPONENTS Core Gui Widgets REQUIRED)

qt_standard_project_setup()

set(CHIRON_ADS_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/Qt-Advanced-Docking-System")
if(NOT EXISTS "${CHIRON_ADS_SOURCE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "Qt-Advanced-Docking-System was not found at ${CHIRON_ADS_SOURCE_DIR}.")
endif()

list(PREPEND CMAKE_MODULE_PATH "${CHIRON_ADS_SOURCE_DIR}/cmake/modules")
set(ADS_VERSION "4.5.0" CACHE STRING "Vendored Qt-Advanced-Docking-System version" FORCE)
set(BUILD_EXAMPLES OFF CACHE BOOL "Disable Qt-Advanced-Docking-System examples" FORCE)
set(BUILD_STATIC ON CACHE BOOL "Build Qt-Advanced-Docking-System statically" FORCE)
add_subdirectory("${CHIRON_ADS_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/qtads")
chiron_gui_stage_target(qtadvanceddocking-qt6)

add_executable(CloganGUI
    src/main.cpp
    assets/clogan_gui_resources.qrc
    src/bug_reporter.cpp
    src/bug_reporter.hpp
    src/main_window.cpp
    src/main_window.hpp
    src/main_window_diagnostics.cpp
    src/main_window_internal.hpp
    src/main_window_members.hpp
    src/main_window_methods.hpp
    src/main_window_optional_fields.cpp
    src/main_window_statistics.cpp
    src/main_window_topology.cpp
    src/main_window_trace.cpp
    src/main_window_ui.cpp
    src/main_window_view.cpp
    src/main_window_xaction_index.cpp
    src/flit_record.hpp
    src/clipboard_widget.cpp
    src/clipboard_widget.hpp
    src/channel_badge_painter.hpp
    src/gui_format.hpp
    src/gui_colors.hpp
    src/flit_transaction_keys.hpp
    src/flit_edit_adapter.cpp
    src/flit_edit_adapter.hpp
    src/latency_analysis.cpp
    src/latency_analysis.hpp
    src/latency_diff_export.cpp
    src/latency_diff_export.hpp
    src/latency_diff_widget.cpp
    src/latency_diff_widget.hpp
    src/latency_widget.cpp
    src/latency_widget.hpp
    src/flit_table_model.cpp
    src/flit_table_model.hpp
    src/flit_details_model.cpp
    src/flit_details_model.hpp
    src/cache_widget.cpp
    src/cache_widget.hpp
    src/address_widget.cpp
    src/address_widget.hpp
    src/timeline_widget.cpp
    src/timeline_widget.hpp
    src/transaction_widget.cpp
    src/transaction_widget.hpp
    src/chiron_b_adapter.hpp
    src/chiron_eb_expresso.hpp
    src/chiron_eb_adapter.hpp
    src/clog_b_trace_loader.cpp
    src/clog_b_trace_loader.hpp
    src/trace_session.cpp
    src/trace_session.hpp
    src/trace_statistics.cpp
    src/trace_statistics.hpp
    src/config.hpp
)

if(APPLE)
    set_target_properties(CloganGUI PROPERTIES MACOSX_BUNDLE TRUE)
endif()

chiron_gui_stage_target(CloganGUI)

if(APPLE)
    set(CHIRON_GUI_LICENSE_OUTPUT_DIR "$<TARGET_FILE_DIR:CloganGUI>/../Resources")
else()
    set(CHIRON_GUI_LICENSE_OUTPUT_DIR "$<TARGET_FILE_DIR:CloganGUI>")
endif()

add_custom_command(TARGET CloganGUI
    POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${CHIRON_GUI_LICENSE_OUTPUT_DIR}"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${CHIRON_GUI_THIRD_PARTY_LICENSE_FILE}"
        "${CHIRON_GUI_LICENSE_OUTPUT_DIR}/THIRD_PARTY_LICENSES.txt"
    COMMENT "Copying third-party license notice"
    VERBATIM
)

if(CHIRON_GUI_BUILD_BENCHMARKS)
    add_executable(CloganGUILoaderBench
        src/clogan_gui_loader_bench.cpp
        src/bug_reporter.cpp
        src/bug_reporter.hpp
        src/flit_record.hpp
        src/flit_transaction_keys.hpp
        src/flit_edit_adapter.cpp
        src/flit_edit_adapter.hpp
        src/gui_format.hpp
        src/channel_badge_painter.hpp
        src/gui_colors.hpp
        src/cache_widget.cpp
        src/cache_widget.hpp
        src/flit_table_model.cpp
        src/flit_table_model.hpp
        src/filter_parallel_utils.hpp
        src/transaction_widget.cpp
        src/transaction_widget.hpp
        src/chiron_b_adapter.hpp
        src/chiron_eb_expresso.hpp
        src/chiron_eb_adapter.hpp
        src/clog_b_trace_loader.cpp
        src/clog_b_trace_loader.hpp
        src/trace_session.cpp
        src/trace_session.hpp
        src/trace_session_fast_index_utils.hpp
        src/trace_statistics.cpp
        src/trace_statistics.hpp
        src/config.hpp
    )
    chiron_gui_stage_target(CloganGUILoaderBench)
endif()

if(CHIRON_GUI_BUILD_TESTS)
    enable_testing()

    add_executable(CloganGUIUnitTests
        tests/clogan_gui_critical_tests.cpp
        tests/bug_reporter_test_stub.cpp
        src/bug_reporter.hpp
        src/main_window.cpp
        src/main_window.hpp
        src/main_window_diagnostics.cpp
        src/main_window_internal.hpp
        src/main_window_members.hpp
        src/main_window_methods.hpp
        src/main_window_optional_fields.cpp
        src/main_window_statistics.cpp
        src/main_window_topology.cpp
        src/main_window_trace.cpp
        src/main_window_ui.cpp
        src/main_window_view.cpp
        src/main_window_xaction_index.cpp
        src/flit_record.hpp
        src/clipboard_widget.cpp
        src/clipboard_widget.hpp
        src/channel_badge_painter.hpp
        src/gui_format.hpp
        src/gui_colors.hpp
        src/flit_transaction_keys.hpp
        src/flit_edit_adapter.cpp
        src/flit_edit_adapter.hpp
        src/latency_analysis.cpp
        src/latency_analysis.hpp
        src/latency_diff_export.cpp
        src/latency_diff_export.hpp
        src/latency_diff_widget.cpp
        src/latency_diff_widget.hpp
        src/latency_widget.cpp
        src/latency_widget.hpp
        src/flit_table_model.cpp
        src/flit_table_model.hpp
        src/flit_details_model.cpp
        src/flit_details_model.hpp
        src/cache_widget.cpp
        src/cache_widget.hpp
        src/address_widget.cpp
        src/address_widget.hpp
        src/timeline_widget.cpp
        src/timeline_widget.hpp
        src/transaction_widget.cpp
        src/transaction_widget.hpp
        src/filter_parallel_utils.hpp
        src/chiron_b_adapter.hpp
        src/chiron_eb_expresso.hpp
        src/chiron_eb_adapter.hpp
        src/clog_b_trace_loader.cpp
        src/clog_b_trace_loader.hpp
        src/trace_session.cpp
        src/trace_session.hpp
        src/trace_session_fast_index_utils.hpp
        src/trace_statistics.cpp
        src/trace_statistics.hpp
        src/config.hpp
    )
    chiron_gui_stage_target(CloganGUIUnitTests)
endif()

if(WIN32)
    target_sources(CloganGUI PRIVATE
        assets/clogan_gui_windows_icon.rc
    )
endif()

if(WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    get_filename_component(CHIRON_MINGW_BIN_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)
    find_program(CHIRON_OBJCOPY_EXECUTABLE
        NAMES objcopy
        HINTS "${CHIRON_MINGW_BIN_DIR}"
    )
    find_program(CHIRON_NM_EXECUTABLE
        NAMES nm
        HINTS "${CHIRON_MINGW_BIN_DIR}"
    )
endif()

target_include_directories(CloganGUI
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../..
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

if(CHIRON_GUI_BUILD_BENCHMARKS)
    target_compile_definitions(CloganGUILoaderBench PRIVATE
        CHIRON_GUI_ENABLE_TRANSACTION_TEST_API)
    target_include_directories(CloganGUILoaderBench
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/../..
            ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
endif()

if(CHIRON_GUI_BUILD_TESTS)
    target_compile_definitions(CloganGUIUnitTests PRIVATE
        CHIRON_GUI_ENABLE_TIMELINE_TEST_API
        CHIRON_GUI_ENABLE_ADDRESS_TEST_API
        CHIRON_GUI_ENABLE_CACHE_TEST_API
        CHIRON_GUI_ENABLE_LATENCY_DIFF_TEST_API
        CHIRON_GUI_ENABLE_LATENCY_TEST_API
        CHIRON_GUI_ENABLE_TRANSACTION_TEST_API
        CHIRON_GUI_ENABLE_MAIN_WINDOW_TEST_API)
    target_include_directories(CloganGUIUnitTests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/../..
            ${CMAKE_CURRENT_SOURCE_DIR}/src
            ${CMAKE_CURRENT_SOURCE_DIR}/tests
    )
endif()

target_link_libraries(CloganGUI
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        z
        ads::qtadvanceddocking-qt6
)

if(CHIRON_GUI_BUILD_BENCHMARKS)
    target_link_libraries(CloganGUILoaderBench
        PRIVATE
            Qt6::Core
            Qt6::Gui
            Qt6::Widgets
            z
    )
endif()

if(CHIRON_GUI_BUILD_TESTS)
    target_link_libraries(CloganGUIUnitTests
        PRIVATE
            Qt6::Core
            Qt6::Gui
            Qt6::Widgets
            z
            ads::qtadvanceddocking-qt6
    )
endif()

if(WIN32)
    target_link_libraries(CloganGUI PRIVATE psapi)
    target_link_libraries(CloganGUI PRIVATE dbghelp)
    if(CHIRON_GUI_BUILD_BENCHMARKS)
        target_link_libraries(CloganGUILoaderBench PRIVATE psapi)
        target_link_libraries(CloganGUILoaderBench PRIVATE dbghelp)
    endif()
    if(CHIRON_GUI_BUILD_TESTS)
        target_link_libraries(CloganGUIUnitTests PRIVATE psapi)
        target_link_libraries(CloganGUIUnitTests PRIVATE dbghelp)
    endif()
endif()

if(CHIRON_GUI_ENABLE_QT_RUNTIME_DEPLOYMENT AND (APPLE OR (UNIX AND NOT ANDROID)))
    qt_generate_deploy_app_script(
        TARGET CloganGUI
        OUTPUT_SCRIPT chiron_gui_deploy_script
        NO_COMPILER_RUNTIME
    )
    set(CHIRON_GUI_DEPLOY_SCRIPT "${chiron_gui_deploy_script}" CACHE INTERNAL "Generated deployment script for wrapper-driven Qt runtime deployment" FORCE)
else()
    set(CHIRON_GUI_DEPLOY_SCRIPT "" CACHE INTERNAL "Generated deployment script for wrapper-driven Qt runtime deployment" FORCE)
endif()

if(WIN32 AND CHIRON_GUI_ENABLE_WINDEPLOYQT)
    get_target_property(_qt_qmake_executable Qt6::qmake IMPORTED_LOCATION)

    if(_qt_qmake_executable)
        get_filename_component(_qt_bin_dir "${_qt_qmake_executable}" DIRECTORY)
    endif()

    find_program(CHIRON_WINDEPLOYQT_EXECUTABLE
        NAMES windeployqt6 windeployqt
        HINTS ${_qt_bin_dir}
    )

    find_program(CHIRON_QTPATHS_EXECUTABLE
        NAMES qtpaths6 qtpaths
        HINTS ${_qt_bin_dir}
    )

    if(CHIRON_WINDEPLOYQT_EXECUTABLE AND CHIRON_QTPATHS_EXECUTABLE)
        add_custom_command(TARGET CloganGUI
            POST_BUILD
            COMMAND "${CHIRON_WINDEPLOYQT_EXECUTABLE}"
                --qtpaths "${CHIRON_QTPATHS_EXECUTABLE}"
                --dir "$<TARGET_FILE_DIR:CloganGUI>"
                "$<TARGET_FILE:CloganGUI>"
            COMMENT "Deploying Qt runtime with windeployqt"
            VERBATIM
        )
    else()
        message(WARNING "windeployqt or qtpaths was not found; CloganGUI will be built without automatic Qt runtime deployment.")
    endif()
endif()

if(MSVC)
    target_compile_options(CloganGUI PRIVATE /W4 /permissive-)
    if(CHIRON_GUI_BUILD_BENCHMARKS)
        target_compile_options(CloganGUILoaderBench PRIVATE /W4 /permissive-)
    endif()
else()
    target_compile_options(CloganGUI PRIVATE -Wall -Wextra -Wpedantic)
    if(CHIRON_GUI_BUILD_BENCHMARKS)
        target_compile_options(CloganGUILoaderBench PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    if(CHIRON_GUI_BUILD_TESTS)
        target_compile_options(CloganGUIUnitTests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endif()

if(WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(CHIRON_GUI_DEBUG_SYMBOL_CONFIGS Debug)
    target_compile_options(CloganGUI PRIVATE
        "$<$<CONFIG:Debug>:-g3>"
        "$<$<CONFIG:Debug>:-ggdb3>"
        "$<$<CONFIG:Debug>:-fno-omit-frame-pointer>"
        "$<$<CONFIG:Debug>:-fno-optimize-sibling-calls>"
        "$<$<CONFIG:Debug>:-fvar-tracking-assignments>"
    )
    target_link_options(CloganGUI PRIVATE
        "$<$<CONFIG:Debug>:-Wl,-Map,$<TARGET_FILE_DIR:CloganGUI>/CloganGUI.map>"
    )

    if(CHIRON_OBJCOPY_EXECUTABLE)
        if(CMAKE_CONFIGURATION_TYPES)
            add_custom_command(TARGET CloganGUI
                POST_BUILD
                COMMAND "${CMAKE_COMMAND}"
                    -DCONFIGURATION=$<CONFIG>
                    -DOBJCOPY_EXECUTABLE=${CHIRON_OBJCOPY_EXECUTABLE}
                    -DINPUT_FILE=$<TARGET_FILE:CloganGUI>
                    -DOUTPUT_FILE=$<TARGET_FILE_DIR:CloganGUI>/CloganGUI.debug
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/preserve_mingw_debug_symbols.cmake"
                COMMENT "Preserving standalone debug symbols for CloganGUI"
                VERBATIM
            )
        elseif(CMAKE_BUILD_TYPE IN_LIST CHIRON_GUI_DEBUG_SYMBOL_CONFIGS)
            add_custom_command(TARGET CloganGUI
                POST_BUILD
                COMMAND "${CMAKE_COMMAND}"
                    -DCONFIGURATION=${CMAKE_BUILD_TYPE}
                    -DOBJCOPY_EXECUTABLE=${CHIRON_OBJCOPY_EXECUTABLE}
                    -DINPUT_FILE=$<TARGET_FILE:CloganGUI>
                    -DOUTPUT_FILE=$<TARGET_FILE_DIR:CloganGUI>/CloganGUI.debug
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/preserve_mingw_debug_symbols.cmake"
                COMMENT "Preserving standalone debug symbols for CloganGUI"
                VERBATIM
            )
        endif()
    endif()

    if(CHIRON_NM_EXECUTABLE)
        if(CMAKE_CONFIGURATION_TYPES)
            add_custom_command(TARGET CloganGUI
                POST_BUILD
                COMMAND "${CMAKE_COMMAND}"
                    -DCONFIGURATION=$<CONFIG>
                    -DNM_EXECUTABLE=${CHIRON_NM_EXECUTABLE}
                    -DINPUT_FILE=$<TARGET_FILE:CloganGUI>
                    -DOUTPUT_FILE=$<TARGET_FILE_DIR:CloganGUI>/CloganGUI.symbols.txt
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/export_mingw_symbols.cmake"
                COMMENT "Exporting symbol listing for CloganGUI"
                VERBATIM
            )
        elseif(CMAKE_BUILD_TYPE IN_LIST CHIRON_GUI_DEBUG_SYMBOL_CONFIGS)
            add_custom_command(TARGET CloganGUI
                POST_BUILD
                COMMAND "${CMAKE_COMMAND}"
                    -DCONFIGURATION=${CMAKE_BUILD_TYPE}
                    -DNM_EXECUTABLE=${CHIRON_NM_EXECUTABLE}
                    -DINPUT_FILE=$<TARGET_FILE:CloganGUI>
                    -DOUTPUT_FILE=$<TARGET_FILE_DIR:CloganGUI>/CloganGUI.symbols.txt
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/export_mingw_symbols.cmake"
                COMMENT "Exporting symbol listing for CloganGUI"
                VERBATIM
            )
        endif()
    endif()
endif()

if(CHIRON_GUI_BUILD_TESTS)
    if(WIN32)
        add_test(NAME CloganGUIUnitTests
            COMMAND powershell -ExecutionPolicy Bypass -File
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/run_staged_executable.ps1"
                "$<TARGET_FILE:CloganGUIUnitTests>")
    else()
        add_test(NAME CloganGUIUnitTests COMMAND $<TARGET_FILE:CloganGUIUnitTests>)
    endif()
endif()
