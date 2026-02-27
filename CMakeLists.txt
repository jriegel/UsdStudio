# (c)2024 Juergen Riegel, mail@juergen-riegel.net

cmake_minimum_required(VERSION 3.30.0)

project(UsdStudio)

add_compile_options($<$<CXX_COMPILER_ID:MSVC>:/MP>)

option(BUILD_GUI "Build with GUI" ON )

# Find includes in corresponding build directories
set(CMAKE_INCLUDE_CURRENT_DIR ON) 


find_package (Python3 REQUIRED COMPONENTS Interpreter Development)


find_package(CLI11 CONFIG REQUIRED)
 



# Base libraries and tools =====================================================================
#add_subdirectory(Base)

# All the Visual components =====================================================================
add_subdirectory(App)

# Python binding ===================================================================
if(BUILD_GUI)
  add_subdirectory(Gui)
endif(BUILD_GUI)

