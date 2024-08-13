# (c)2024 Juergen Riegel, mail@juergen-riegel.net

cmake_minimum_required(VERSION 3.28.0)


project(UsdStudio)

add_compile_options($<$<CXX_COMPILER_ID:MSVC>:/MP>)

option(BUILD_GUI "Build with GUI" ON )

# Find includes in corresponding build directories
set(CMAKE_INCLUDE_CURRENT_DIR ON) 


find_package (Python3 COMPONENTS Interpreter Development)

# Find the QtWidgets library
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Network Quick QuickControls2 QuickWidgets)
#qt_standard_project_setup()

# import the targets of USD
include(thirdParty/USD-Build/cmake/pxrTargets.cmake)

find_package(CLI11 CONFIG REQUIRED)
 
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)



# Base libraries and tools =====================================================================
#add_subdirectory(Base)

# All the Visual components =====================================================================
add_subdirectory(App)

# Python binding ===================================================================
if(BUILD_GUI)
  add_subdirectory(Gui)
endif(BUILD_GUI)

