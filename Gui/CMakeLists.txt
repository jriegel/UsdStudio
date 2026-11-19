# (c)2024 Juergen Riegel, mail@juergen-riegel.net

# Find the QtWidgets library
# Note: no "GuiPrivate" COMPONENT here - this Qt packaging defines the
# Qt6::GuiPrivate target from Qt6Gui but ships no Qt6GuiPrivateConfig.cmake.
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Network Quick QuickControls2 QuickWidgets ShaderTools)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)


# Models =====================================================================
add_subdirectory(Src)

# Unit tests =====================================================================
add_subdirectory(Tests)

# Exe project =====================================================================
add_subdirectory(Main)
