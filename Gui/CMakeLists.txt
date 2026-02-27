# (c)2024 Juergen Riegel, mail@juergen-riegel.net

# Find the QtWidgets library
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Network Quick QuickControls2 QuickWidgets)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)


# Models =====================================================================
add_subdirectory(Src)

# Exe project =====================================================================
add_subdirectory(Main)
