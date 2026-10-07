cmake_minimum_required(VERSION 3.24)

project(faceRecognition LANGUAGES CXX RC)

if(NOT WIN32)
    message(FATAL_ERROR "This project only supports Windows.")
endif()

if(NOT MSVC)
    message(FATAL_ERROR "This project requires MSVC with MFC; MinGW is not supported.")
endif()

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR "This project must be configured for Win32 (x86), not x64.")
endif()

# Use MFC in the shared DLL and the matching dynamic MSVC runtime.
set(CMAKE_MFC_FLAG 2)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")

set(APP_RUNTIME_DIR "${CMAKE_BINARY_DIR}/bin/$<CONFIG>")
set(PLUGIN_RUNTIME_DIR "${APP_RUNTIME_DIR}/Plugin")
set(IMPORT_LIBRARY_DIR "${CMAKE_BINARY_DIR}/lib/$<CONFIG>")

function(configure_legacy_mfc_target target)
    target_compile_definitions(${target} PRIVATE _MBCS)
    target_compile_options(${target} PRIVATE /W3)
    set_target_properties(${target} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${APP_RUNTIME_DIR}"
        LIBRARY_OUTPUT_DIRECTORY "${APP_RUNTIME_DIR}"
        ARCHIVE_OUTPUT_DIRECTORY "${IMPORT_LIBRARY_DIR}"
    )
endfunction()

function(add_face_plugin target output_name source_dir source_file)
    add_library(${target} SHARED
        "${source_dir}/StdAfx.cpp"
        "${source_dir}/${source_file}.cpp"
        "${source_dir}/${source_file}.h"
        "${source_dir}/Resource.h"
        "${source_dir}/${source_file}.rc"
    )
    target_include_directories(${target} PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${source_dir}"
    )
    target_compile_definitions(${target} PRIVATE _USRDLL)
    configure_legacy_mfc_target(${target})
    set_target_properties(${target} PROPERTIES
        OUTPUT_NAME "${output_name}"
        RUNTIME_OUTPUT_DIRECTORY "${PLUGIN_RUNTIME_DIR}"
        LIBRARY_OUTPUT_DIRECTORY "${PLUGIN_RUNTIME_DIR}"
    )
endfunction()

add_library(ImageProc SHARED
    ImageProc/StdAfx.cpp
    ImageProc/ImageProc.cpp
    ImageProc/ImageProc.h
    ImageProc/Resource.h
    ImageProc/ImageProc.rc
)
target_include_directories(ImageProc PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/ImageProc"
)
target_compile_definitions(ImageProc PRIVATE _USRDLL)
configure_legacy_mfc_target(ImageProc)

add_library(TraceFeature SHARED
    TraceFeature/StdAfx.cpp
    TraceFeature/TraceFeature.cpp
    TraceFeature/TraceFeature.h
    TraceFeature/TraceFeatureApi.h
    TraceFeature/Resource.h
    TraceFeature/TraceFeature.rc
)
target_include_directories(TraceFeature PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/TraceFeature"
)
target_compile_definitions(TraceFeature PRIVATE _USRDLL TRACEFEATURE_EXPORTS)
target_link_libraries(TraceFeature PRIVATE ImageProc)
configure_legacy_mfc_target(TraceFeature)

add_face_plugin(
    ImagePrepare
    PA_ImagePrepare
    "${CMAKE_CURRENT_SOURCE_DIR}/PlugIns/ImagePrepare"
    ImagePrepare
)
target_link_libraries(ImagePrepare PRIVATE ImageProc TraceFeature)

add_face_plugin(
    FaceLocator
    PB_FaceLocator
    "${CMAKE_CURRENT_SOURCE_DIR}/PlugIns/FaceLocator"
    FaceLocator
)
target_link_libraries(FaceLocator PRIVATE ImageProc)

add_face_plugin(
    TraceObject
    PC_TraceObject
    "${CMAKE_CURRENT_SOURCE_DIR}/PlugIns/TraceObject"
    TraceObject
)
target_link_libraries(TraceObject PRIVATE ImageProc TraceFeature)

add_face_plugin(
    BlinkEyeCheck
    PD_BlinkEyeCheck
    "${CMAKE_CURRENT_SOURCE_DIR}/PlugIns/BlinkEyeCheck"
    BlinkEyeCheck
)
target_link_libraries(BlinkEyeCheck PRIVATE ImageProc TraceFeature)

add_face_plugin(
    FinalProc
    PZ_FinalProc
    "${CMAKE_CURRENT_SOURCE_DIR}/PlugIns/FinalProc"
    FinalProc
)
target_link_libraries(FinalProc PRIVATE ImageProc)

add_executable(VFWCAPTURE WIN32
    StdAfx.cpp
    VFWCAPTURE.cpp
    VFWCAPTUREDlg.cpp
    VFWCAPTURE.h
    VFWCAPTUREDlg.h
    Resource.h
    BufStruct.h
    ImageProc.h
    VFWCAPTURE.rc
)
target_include_directories(VFWCAPTURE PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_definitions(VFWCAPTURE PRIVATE FACE_RECOGNITION_CMAKE_BUILD)
target_link_libraries(VFWCAPTURE PRIVATE ImageProc vfw32)
configure_legacy_mfc_target(VFWCAPTURE)
