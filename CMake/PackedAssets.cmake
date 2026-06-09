# Defines: playfultones_packedassets_add_tests(), playfultones_packedassets_add_pack()
set(PT_PACKEDASSETS_DIR "${CMAKE_CURRENT_LIST_DIR}/.." CACHE INTERNAL "")

function(playfultones_packedassets_add_tests)
    if(TARGET playfultones_packedassets_tests)
        return()
    endif()
    add_executable(playfultones_packedassets_tests
        ${PT_PACKEDASSETS_DIR}/tests/PackedAssetsTests.cpp)
    target_compile_features(playfultones_packedassets_tests PRIVATE cxx_std_20)
    target_link_libraries(playfultones_packedassets_tests PRIVATE
        playfultones_packedassets Catch2::Catch2WithMain)
    target_include_directories(playfultones_packedassets_tests PRIVATE
        ${PT_PACKEDASSETS_DIR})
    # The tests exercise collectEntries(), which (like the packer) uses
    # std::filesystem; libc++ marks those APIs unavailable below macOS 10.15.
    # The global deployment target (10.13) is irrelevant for a host test
    # binary, so bump it here. See _pt_pa_ensure_packer for the same rationale:
    # a later -mmacosx-version-min wins on clang, overriding the global flag.
    if(APPLE)
        target_compile_options(playfultones_packedassets_tests PRIVATE -mmacosx-version-min=10.15)
        target_link_options(playfultones_packedassets_tests PRIVATE -mmacosx-version-min=10.15)
    endif()
endfunction()

# ---------------------------------------------------------------------------
# _pt_pa_ensure_packer: build the host packer executable (once).
# ---------------------------------------------------------------------------
function(_pt_pa_ensure_packer)
    if(TARGET pt_pak_packer)
        return()
    endif()
    add_executable(pt_pak_packer
        ${PT_PACKEDASSETS_DIR}/tools/pak_packer.cpp
        ${PT_PACKEDASSETS_DIR}/src/Packer.cpp
        ${PT_PACKEDASSETS_DIR}/src/Crypto.cpp
        ${PT_PACKEDASSETS_DIR}/third_party/tiny-aes/aes.c)
    target_compile_features(pt_pak_packer PRIVATE cxx_std_20)
    target_include_directories(pt_pak_packer PRIVATE ${PT_PACKEDASSETS_DIR})
    # The packer is a host build tool, not a shipped binary. It uses
    # std::filesystem::recursive_directory_iterator, which libc++ marks
    # unavailable below macOS 10.15. The project's global deployment target
    # (10.13) is irrelevant for a build-time tool, so bump it here.
    #
    # The OSX_DEPLOYMENT_TARGET target property does not reliably override the
    # global -mmacosx-version-min flag that CMAKE_OSX_DEPLOYMENT_TARGET injects,
    # so append the flag explicitly (later -mmacosx-version-min wins on clang).
    if(APPLE)
        target_compile_options(pt_pak_packer PRIVATE -mmacosx-version-min=10.15)
        target_link_options(pt_pak_packer PRIVATE -mmacosx-version-min=10.15)
    endif()
endfunction()

# ---------------------------------------------------------------------------
# _pt_pa_write_key_header: convert a 64-hex KEY into the generated header.
# ---------------------------------------------------------------------------
function(_pt_pa_write_key_header KEY OUT_DIR)
    string(LENGTH "${KEY}" _len)
    if(NOT _len EQUAL 64)
        message(FATAL_ERROR "playfultones_packedassets: KEY must be 64 hex chars (32 bytes)")
    endif()
    set(_bytes "")
    foreach(i RANGE 0 31)
        math(EXPR _o "${i}*2")
        string(SUBSTRING "${KEY}" ${_o} 2 _b)
        string(APPEND _bytes "0x${_b},")
    endforeach()
    set(PT_KEY_BYTES "${_bytes}")
    configure_file(${PT_PACKEDASSETS_DIR}/CMake/GeneratedKey.h.in
                   ${OUT_DIR}/GeneratedKey.h @ONLY)
endfunction()

# ---------------------------------------------------------------------------
# _pt_pa_embed_macos: copy pak into each JUCE bundle's Resources after build.
#
# Handles two shapes:
#  - JUCE plugin targets, where the real bundles are the per-format
#    sub-targets ${TARGET}_${fmt} (Standalone/VST3/AU/...).
#  - A plain app/bundle target (e.g. juce_add_gui_app), where ${TARGET}
#    itself is the bundle. Used when FORMATS is empty or none of the
#    sub-targets exist.
# ---------------------------------------------------------------------------
function(_pt_pa_embed_macos TARGET PAK FORMATS)
    set(_embedded_any FALSE)
    foreach(fmt ${FORMATS})
        set(_t ${TARGET}_${fmt})
        if(NOT TARGET ${_t})
            continue()
        endif()
        add_custom_command(TARGET ${_t} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    ${PAK} "$<TARGET_BUNDLE_CONTENT_DIR:${_t}>/Resources/assets.pak"
            COMMENT "Embedding assets.pak into ${_t} bundle Resources")
        add_dependencies(${_t} ${TARGET}_pack)
        set(_embedded_any TRUE)
    endforeach()

    if(_embedded_any)
        return()
    endif()

    # No plugin-format sub-targets matched: fall back to embedding directly
    # into the target itself, provided it is an app/bundle target.
    if(NOT TARGET ${TARGET})
        return()
    endif()
    get_target_property(_is_bundle ${TARGET} MACOSX_BUNDLE)
    get_target_property(_ttype ${TARGET} TYPE)
    if(_is_bundle OR _ttype STREQUAL "EXECUTABLE")
        add_custom_command(TARGET ${TARGET} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    ${PAK} "$<TARGET_BUNDLE_CONTENT_DIR:${TARGET}>/Resources/assets.pak"
            COMMENT "Embedding assets.pak into ${TARGET} bundle Resources")
        add_dependencies(${TARGET} ${TARGET}_pack)
    endif()
endfunction()

# ---------------------------------------------------------------------------
# _pt_pa_embed_windows: add a generated .rc file with RCDATA to the target.
# ---------------------------------------------------------------------------
function(_pt_pa_embed_windows TARGET PAK RESOURCE_NAME)
    set(PT_RESOURCE_NAME ${RESOURCE_NAME})
    file(TO_NATIVE_PATH "${PAK}" PT_PAK_PATH)
    string(REPLACE "\\" "\\\\" PT_PAK_PATH "${PT_PAK_PATH}")
    set(_rc ${CMAKE_BINARY_DIR}/playfultones_packedassets_gen/PackedAssets.rc)
    configure_file(${PT_PACKEDASSETS_DIR}/windows/PackedAssetsResource.rc.in ${_rc} @ONLY)
    set_source_files_properties(${_rc} PROPERTIES GENERATED TRUE
        OBJECT_DEPENDS ${PAK})
    target_sources(${TARGET} PRIVATE ${_rc})
    add_dependencies(${TARGET} ${TARGET}_pack)
endfunction()

# ---------------------------------------------------------------------------
# playfultones_packedassets_add_pack: wire packing + embedding for a target.
#
# Required: TARGET ASSET_DIR KEY
# Optional: PAK_NAME (default: assets.pak)
#           RESOURCE_NAME (default: PACKEDASSETS)
#           SCAN (ALWAYS|CONFIGURE, default: ALWAYS)
#           FORMATS (list of JUCE format suffixes, e.g. "Standalone;VST3;AU")
#
# NOTE: TARGET must be the target that COMPILES the module sources. JUCE
#       modules are INTERFACE libraries compiled into the consuming target, and
#       the decryption key is force-included onto TARGET (PRIVATE), so it only
#       reaches code compiled as part of TARGET. Use your SharedCode/plugin
#       target (the one you link the module into), not a sibling.
# ---------------------------------------------------------------------------
function(playfultones_packedassets_add_pack)
    cmake_parse_arguments(PA "" "TARGET;KEY;PAK_NAME;RESOURCE_NAME;SCAN" "FORMATS;ASSET_DIR" ${ARGN})
    if(NOT PA_PAK_NAME)
        set(PA_PAK_NAME assets.pak)
    endif()
    if(NOT PA_RESOURCE_NAME)
        set(PA_RESOURCE_NAME PACKEDASSETS)
    endif()
    if(NOT PA_SCAN)
        set(PA_SCAN ALWAYS)
    endif()

    _pt_pa_ensure_packer()
    set(_gen ${CMAKE_BINARY_DIR}/playfultones_packedassets_gen)
    file(MAKE_DIRECTORY ${_gen})
    _pt_pa_write_key_header("${PA_KEY}" "${_gen}")
    # Override the committed fallback key with the generated one.
    #
    # Source.cpp does `#include "GeneratedKey.h"`, a quoted include that
    # resolves relative to Source.cpp's own directory FIRST (the committed
    # {0} fallback), before any -I path is consulted. So adding ${_gen} to the
    # include path is not enough to shadow it. Force-include the generated
    # header ahead of every TU instead: it defines PT_PACKEDASSETS_KEY, and
    # the committed header's #ifndef guard then skips its {0} default.
    if(MSVC)
        target_compile_options(${PA_TARGET} BEFORE PRIVATE /FI"${_gen}/GeneratedKey.h")
    else()
        target_compile_options(${PA_TARGET} BEFORE PRIVATE -include "${_gen}/GeneratedKey.h")
    endif()

    # Build a repeated `--dir <d>` argument list, one per ASSET_DIR root.
    set(_dirargs "")
    foreach(d ${PA_ASSET_DIR})
        list(APPEND _dirargs --dir ${d})
    endforeach()

    set(_pak ${CMAKE_BINARY_DIR}/${PA_PAK_NAME})
    if(PA_SCAN STREQUAL ALWAYS)
        # Always-run: a custom target re-packs every build; packer is cheap and
        # the embed step only refires if the pak content changed.
        add_custom_target(${PA_TARGET}_pack ALL
            COMMAND pt_pak_packer --key ${PA_KEY} --out ${_pak} ${_dirargs}
            BYPRODUCTS ${_pak}
            COMMENT "Packing assets -> ${PA_PAK_NAME}")
        add_dependencies(${PA_TARGET}_pack pt_pak_packer)
    else() # CONFIGURE: glob with CONFIGURE_DEPENDS, repack only when list changes
        set(_assets "")
        foreach(d ${PA_ASSET_DIR})
            file(GLOB_RECURSE _d_assets CONFIGURE_DEPENDS ${d}/*)
            list(APPEND _assets ${_d_assets})
        endforeach()
        add_custom_command(OUTPUT ${_pak}
            COMMAND pt_pak_packer --key ${PA_KEY} --out ${_pak} ${_dirargs}
            DEPENDS ${_assets} pt_pak_packer
            COMMENT "Packing assets -> ${PA_PAK_NAME}")
        add_custom_target(${PA_TARGET}_pack ALL DEPENDS ${_pak})
    endif()

    if(APPLE)
        _pt_pa_embed_macos(${PA_TARGET} ${_pak} "${PA_FORMATS}")
    elseif(WIN32)
        _pt_pa_embed_windows(${PA_TARGET} ${_pak} ${PA_RESOURCE_NAME})
    endif()
endfunction()

# ---------------------------------------------------------------------------
# playfultones_packedassets_add_smoke: minimal end-to-end smoke app.
#
# Behind the PT_PACKEDASSETS_SMOKE option, builds a tiny JUCE GUI app that
# loads the embedded pak at runtime and decodes an image. Exercises the full
# chain: packer -> bundle Resources -> createDefaultSource() mmap -> decode.
# ---------------------------------------------------------------------------
function(playfultones_packedassets_add_smoke)
    if(TARGET playfultones_packedassets_smoke)
        return()
    endif()

    juce_add_gui_app(playfultones_packedassets_smoke
        PRODUCT_NAME "pa_smoke"
        BUNDLE_ID com.playfultones.packedassets.smoke)

    juce_generate_juce_header(playfultones_packedassets_smoke)

    target_sources(playfultones_packedassets_smoke PRIVATE
        ${PT_PACKEDASSETS_DIR}/tests/SmokeMain.cpp)

    target_compile_features(playfultones_packedassets_smoke PRIVATE cxx_std_20)

    target_compile_definitions(playfultones_packedassets_smoke PRIVATE
        JUCE_STANDALONE_APPLICATION=1
        JUCE_USE_CURL=0
        JUCE_WEB_BROWSER=0)

    target_link_libraries(playfultones_packedassets_smoke PRIVATE
        playfultones_packedassets
        juce::juce_gui_extra
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

    playfultones_packedassets_add_pack(
        TARGET playfultones_packedassets_smoke
        ASSET_DIR ${PT_PACKEDASSETS_DIR}/tests/smoke_assets
        KEY 00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff)
endfunction()
