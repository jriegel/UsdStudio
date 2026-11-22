# (c)2024 Juergen Riegel, mail@juergen-riegel.net

cmake_minimum_required(VERSION 3.30.0)

project(UsdStudio)

get_property(is_multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(is_multi_config)
  set(CMAKE_CONFIGURATION_TYPES "Release;RelWithDebInfo" CACHE STRING
    "Build configurations compatible with the OpenUSD release build" FORCE)
endif()

add_compile_options($<$<CXX_COMPILER_ID:MSVC>:/MP>)

option(BUILD_GUI "Build with GUI" ON )

enable_testing()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  add_test(NAME VulkanSdkTests
    COMMAND bash "${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/Tests/VulkanSdkTests.sh")
endif()

# Find includes in corresponding build directories
set(CMAKE_INCLUDE_CURRENT_DIR ON) 


#find_package (Python3 REQUIRED COMPONENTS Interpreter Development)

set(OPENUSD_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/OpenUsd_Build_Release")
if(NOT EXISTS "${OPENUSD_ROOT}/pxrConfig.cmake")
  message(FATAL_ERROR
    "OpenUSD was not found at ${OPENUSD_ROOT}. Run Setup.bat or Setup.sh to build it first.")
endif()

if(NOT OpenSubdiv_DIR)
  set(OpenSubdiv_DIR "${OPENUSD_ROOT}/lib/cmake/OpenSubdiv")
endif()

if(NOT TBB_DIR AND EXISTS "${OPENUSD_ROOT}/lib/cmake/TBB")
  set(TBB_DIR "${OPENUSD_ROOT}/lib/cmake/TBB")
endif()

# pxrTargets reference OpenGL::GL (garch/hgiGL/usdImagingGL); the package
# config does not find_dependency(OpenGL) itself, so resolve it first.
find_package(OpenGL REQUIRED)
find_package(pxr CONFIG REQUIRED PATHS "${OPENUSD_ROOT}" NO_DEFAULT_PATH)

add_library(UsdStudioOpenUSD INTERFACE)
target_link_libraries(UsdStudioOpenUSD INTERFACE
  ar
  arch
  gf
  js
  kind
  pcp
  plug
  sdf
  sdr
  tf
  trace
  usd
  usdGeom
  usdHydra
  usdLux
  usdMedia
  usdPhysics
  usdRender
  usdShade
  usdSkel
  usdUI
  usdUtils
  usdVol
  vt
  work
)

if(BUILD_GUI)
  foreach(usd_target IN ITEMS hd hdSt hgi usdImaging usdImagingGL)
    if(NOT TARGET ${usd_target})
      message(FATAL_ERROR
        "OpenUSD is missing ${usd_target}. Rebuild OpenUSD with USD imaging; see Build.md.")
    endif()
  endforeach()

  if(TARGET hgiVulkan)
    find_package(Vulkan REQUIRED COMPONENTS shaderc_combined)
    set(USDSTUDIO_HGI_VULKAN hgiVulkan)
  elseif(WIN32 AND EXISTS "${OPENUSD_ROOT}/lib/usd_hgiVulkan.lib")
    add_library(UsdStudioHgiVulkan SHARED IMPORTED GLOBAL)
    set_target_properties(UsdStudioHgiVulkan PROPERTIES
      IMPORTED_IMPLIB "${OPENUSD_ROOT}/lib/usd_hgiVulkan.lib"
      IMPORTED_LOCATION "${OPENUSD_ROOT}/lib/usd_hgiVulkan.dll")
    target_include_directories(UsdStudioHgiVulkan INTERFACE "${OPENUSD_ROOT}/include")
    set(USDSTUDIO_HGI_VULKAN UsdStudioHgiVulkan)
  elseif(UNIX AND NOT APPLE AND EXISTS "${OPENUSD_ROOT}/lib/libusd_hgiVulkan.so")
    find_package(Vulkan REQUIRED COMPONENTS shaderc_combined)
    add_library(UsdStudioHgiVulkan SHARED IMPORTED GLOBAL)
    set_target_properties(UsdStudioHgiVulkan PROPERTIES
      IMPORTED_LOCATION "${OPENUSD_ROOT}/lib/libusd_hgiVulkan.so")
    target_include_directories(UsdStudioHgiVulkan INTERFACE "${OPENUSD_ROOT}/include")
    target_link_libraries(UsdStudioHgiVulkan INTERFACE Vulkan::Vulkan)
    set(USDSTUDIO_HGI_VULKAN UsdStudioHgiVulkan)
  else()
    message(FATAL_ERROR
      "OpenUSD is missing HgiVulkan. Build it separately or rebuild OpenUSD with Vulkan support; see Build.md.")
  endif()

  add_library(UsdStudioImaging INTERFACE)
  target_include_directories(UsdStudioImaging INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/vma/include")
  target_link_libraries(UsdStudioImaging INTERFACE
    garch geomUtil glf cameraUtil hd hdMtlx hdSt hdsi hdx
    hgi hgiGL hgiInterop usdImaging usdImagingGL ${USDSTUDIO_HGI_VULKAN})
endif()

# The CLI front-end parses its own arguments (see App/src/Private/Cli), so no
# third-party CLI parser is needed here.
 



# Base libraries and tools =====================================================================
#add_subdirectory(Base)

# All the Visual components =====================================================================
add_subdirectory(App)

# Python binding ===================================================================
if(BUILD_GUI)
  add_subdirectory(Gui)
endif(BUILD_GUI)
