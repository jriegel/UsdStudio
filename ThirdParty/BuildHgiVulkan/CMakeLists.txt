# (c)2026 Juergen Riegel, mail@juergen-riegel.net
#
# Builds ONLY pxr/imaging/hgiVulkan (the Vulkan backend of the Hydra Graphics
# Interface) from the OpenUSD source tree and links it against an already
# installed OpenUSD.  A stock OpenUSD install has no HgiVulkan, and enabling it
# through build_usd.py (--vulkan) rebuilds every pxr library, which takes
# hours.  This mini project keeps the Vulkan backend a small, targeted add-on
# that is installed back into the same tree (see ThirdParty/BuildHgiVulkan.bat).
#
# Requires VULKAN_SDK to point at an installed Vulkan SDK; vulkan-1 and
# shaderc_combined are taken from there.

cmake_minimum_required(VERSION 3.30)

project(UsdStudioHgiVulkan LANGUAGES CXX)

set(OPENUSD_SRC "" CACHE PATH "OpenUSD source tree (the directory that contains pxr/)")
set(OPENUSD_ROOT "" CACHE PATH "Installed OpenUSD tree (the one UsdStudio links against)")

foreach(var OPENUSD_SRC OPENUSD_ROOT)
  if(NOT IS_DIRECTORY "${${var}}")
    message(FATAL_ERROR "${var} is not set to a valid directory: '${${var}}'")
  endif()
endforeach()

if(NOT IS_DIRECTORY "${OPENUSD_SRC}/pxr/imaging/hgiVulkan")
  message(FATAL_ERROR "No pxr/imaging/hgiVulkan under ${OPENUSD_SRC}")
endif()

if(NOT "$ENV{VULKAN_SDK}" MATCHES ".")
  message(FATAL_ERROR "The VULKAN_SDK environment variable is not set.")
endif()

find_package(Vulkan REQUIRED COMPONENTS shaderc_combined)

if(NOT OpenSubdiv_DIR)
  set(OpenSubdiv_DIR "${OPENUSD_ROOT}/lib/cmake/OpenSubdiv")
endif()
if(NOT TBB_DIR AND EXISTS "${OPENUSD_ROOT}/lib/cmake/TBB")
  set(TBB_DIR "${OPENUSD_ROOT}/lib/cmake/TBB")
endif()
# The imported pxr target "garch" references OpenGL::GL in its interface.
find_package(OpenGL REQUIRED)
find_package(pxr CONFIG REQUIRED PATHS "${OPENUSD_ROOT}" NO_DEFAULT_PATH)

# CONFIGURE_DEPENDS so upstream source changes show up on a rebuild.
file(GLOB HGIVULKAN_SOURCES CONFIGURE_DEPENDS "${OPENUSD_SRC}/pxr/imaging/hgiVulkan/*.cpp")

# ---------------------------------------------------------------------------
# usd_hgi and usd_hgiInterop are rebuilt here as well, with
# PXR_VULKAN_SUPPORT_ENABLED defined.  The stock install was configured with
# PXR_ENABLE_VULKAN_SUPPORT=OFF, so Hgi::_MakeNewPlatformDefaultHgi() ignores
# the HGI_ENABLE_VULKAN env var (it would hit a TF_CODING_ERROR) and Hydra
# falls back to the platform default Hgi (HgiGL).  Hydra's
# HdRendererPlugin::CreateDelegate() re-checks support with *empty* create
# args, so a headless app without OpenGL fails there.  Defining the macro in
# those two libraries only changes hgi.cpp/hgiInterop.cpp function bodies - no
# ABI, no export changes - so the rebuild is drop-in compatible with the rest
# of the install.
# ---------------------------------------------------------------------------
file(GLOB HGI_SOURCES CONFIGURE_DEPENDS "${OPENUSD_SRC}/pxr/imaging/hgi/*.cpp")
# UsdStudio tweak: HdRendererPlugin::CreateDelegate() re-checks renderer
# support with empty create args, which probes the *platform default* Hgi.
# On Windows that is HgiGL, and without an OpenGL context the probe fails
# silently - Storm never attaches even when the client passed an HgiVulkan.
# Keep the upstream tree pristine but flip the HGI_ENABLE_VULKAN default to
# 1 in a generated copy of hgi.cpp, so the platform default IS HgiVulkan in
# this install.  (HGI_ENABLE_VULKAN=0 in the environment still opts out.)
file(READ "${OPENUSD_SRC}/pxr/imaging/hgi/hgi.cpp" HGI_CPP)
string(REPLACE "TF_DEFINE_ENV_SETTING(HGI_ENABLE_VULKAN, 0,"
               "TF_DEFINE_ENV_SETTING(HGI_ENABLE_VULKAN, 1,"
               HGI_CPP_PATCHED "${HGI_CPP}")
if(HGI_CPP_PATCHED STREQUAL HGI_CPP)
  message(FATAL_ERROR
    "Did not find 'TF_DEFINE_ENV_SETTING(HGI_ENABLE_VULKAN, 0,' in "
    "${OPENUSD_SRC}/pxr/imaging/hgi/hgi.cpp - check the upstream source.")
endif()
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/hgi/hgi.cpp"
      CONTENT "${HGI_CPP_PATCHED}")
list(FILTER HGI_SOURCES EXCLUDE REGEX "imaging/hgi/hgi\\.cpp$")
add_library(usd_hgi SHARED ${HGI_SOURCES}
            "${CMAKE_CURRENT_BINARY_DIR}/hgi/hgi.cpp")
file(GLOB HGIINTEROP_SOURCES CONFIGURE_DEPENDS "${OPENUSD_SRC}/pxr/imaging/hgiInterop/*.cpp")
add_library(usd_hgiInterop SHARED ${HGIINTEROP_SOURCES})

foreach(tgt usd_hgi usd_hgiInterop)
  set_target_properties(${tgt} PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
  target_include_directories(${tgt} PRIVATE
    "${OPENUSD_SRC}"
    "${CMAKE_CURRENT_SOURCE_DIR}/../vma/include")
  target_compile_definitions(${tgt} PRIVATE
    WIN32
    NDEBUG
    PXR_GL_SUPPORT_ENABLED
    PXR_MATERIALX_SUPPORT_ENABLED
    PXR_VULKAN_SUPPORT_ENABLED
    PXR_BUILD_LOCATION=usd
    PXR_PLUGIN_BUILD_LOCATION=../plugin/usd
    BOOST_ALL_DYN_LINK
    BOOST_ALL_NO_LIB
    OPENEXR_DLL
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    _CRT_SECURE_NO_WARNINGS
    _CRT_NONSTDC_NO_WARNINGS
    _SCL_SECURE_NO_WARNINGS
    _SILENCE_CXX17_ITERATOR_BASE_CLASS_DEPRECATION_WARNING)
  if(MSVC)
    target_compile_options(${tgt} PRIVATE /MP /Zc:__cplusplus /bigobj /Zm150)
  endif()
endforeach()

target_compile_definitions(usd_hgi PRIVATE
  MFB_PACKAGE_NAME=hgi MFB_ALT_PACKAGE_NAME=hgi MFB_PACKAGE_MODULE=Hgi
  HGI_EXPORTS)
target_compile_definitions(usd_hgiInterop PRIVATE
  MFB_PACKAGE_NAME=hgiInterop MFB_ALT_PACKAGE_NAME=hgiInterop
  MFB_PACKAGE_MODULE=HgiInterop
  HGIINTEROP_EXPORTS)

# Same libraries the packages' own CMakeLists.txt ask for.
target_link_libraries(usd_hgi PRIVATE arch gf hio plug tf trace)
target_link_libraries(usd_hgiInterop PRIVATE garch gf hgi tf vt Vulkan::Vulkan)

add_library(usd_hgiVulkan SHARED ${HGIVULKAN_SOURCES})
set_target_properties(usd_hgiVulkan PROPERTIES
  # Matches the naming of the installed pxr libraries (usd_hgi.dll, ...), so the
  # app can link "usd_hgiVulkan" like every other Hydra library.
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED ON)

target_include_directories(usd_hgiVulkan PRIVATE
  # First: the source tree, so the package's own private headers resolve.
  "${OPENUSD_SRC}"
  # Vulkan Memory Allocator, see ThirdParty/vma/README.md.
  "${CMAKE_CURRENT_SOURCE_DIR}/../vma/include")

target_compile_definitions(usd_hgiVulkan PRIVATE
  WIN32
  NDEBUG
  # Mirror the defines the installed pxr libraries were built with; anything
  # that could change their ABI has to match.
  PXR_GL_SUPPORT_ENABLED
  PXR_MATERIALX_SUPPORT_ENABLED
  PXR_VULKAN_SUPPORT_ENABLED
  PXR_BUILD_LOCATION=usd
  PXR_PLUGIN_BUILD_LOCATION=../plugin/usd
  BOOST_ALL_DYN_LINK
  BOOST_ALL_NO_LIB
  OPENEXR_DLL
  WIN32_LEAN_AND_MEAN
  NOMINMAX
  _CRT_SECURE_NO_WARNINGS
  _CRT_NONSTDC_NO_WARNINGS
  _SCL_SECURE_NO_WARNINGS
  _SILENCE_CXX17_ITERATOR_BASE_CLASS_DEPRECATION_WARNING
  MFB_PACKAGE_NAME=hgiVulkan
  MFB_ALT_PACKAGE_NAME=hgiVulkan
  MFB_PACKAGE_MODULE=HgiVulkan
  HGIVULKAN_EXPORTS)

if(MSVC)
  target_compile_options(usd_hgiVulkan PRIVATE /MP /Zc:__cplusplus /bigobj /Zm150)
endif()

# Same libraries hgiVulkan's own CMakeLists.txt asks for.
target_link_libraries(usd_hgiVulkan PRIVATE
  arch
  hgi
  tf
  trace
  Vulkan::Vulkan
  Vulkan::shaderc_combined)

target_link_libraries(usd_hgiInterop PRIVATE usd_hgiVulkan)

# Install into the OpenUSD tree with the layout the rest of the install uses.
install(TARGETS usd_hgi usd_hgiInterop usd_hgiVulkan
  RUNTIME DESTINATION lib     # pxr DLLs live in lib/
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib)

# hgiVulkan registers its Hgi-derived type through a plugInfo.json next to the
# other pxr plugin resources; pxr_library() would expand @PLUG_INFO_*@ macros,
# replicate the installed form (cf. lib/usd/hgiGL/resources/plugInfo.json).
file(READ "${OPENUSD_SRC}/pxr/imaging/hgiVulkan/plugInfo.json" HGIVULKAN_PLUGINFO)
string(REPLACE "@PLUG_INFO_LIBRARY_PATH@" "../../usd_hgiVulkan.dll" HGIVULKAN_PLUGINFO "${HGIVULKAN_PLUGINFO}")
string(REPLACE "@PLUG_INFO_RESOURCE_PATH@" "resources" HGIVULKAN_PLUGINFO "${HGIVULKAN_PLUGINFO}")
string(REPLACE "@PLUG_INFO_ROOT@" ".." HGIVULKAN_PLUGINFO "${HGIVULKAN_PLUGINFO}")
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/plugInfo/usd/hgiVulkan/resources/plugInfo.json"
  CONTENT "${HGIVULKAN_PLUGINFO}")
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/plugInfo/usd/hgiVulkan/resources/plugInfo.json"
  DESTINATION "lib/usd/hgiVulkan/resources")

install(DIRECTORY "${OPENUSD_SRC}/pxr/imaging/hgiVulkan/"
  DESTINATION "include/pxr/imaging/hgiVulkan"
  FILES_MATCHING PATTERN "*.h"
  PATTERN "testenv" EXCLUDE)

# hgiVulkan's public headers include <vma/vk_mem_alloc.h> (the Vulkan SDK used
# to ship it), so consumers of the installed headers need the vendored copy too.
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../vma/include/vma"
  DESTINATION "include")
