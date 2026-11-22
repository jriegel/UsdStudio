# (c)2024 Juergen Riegel, mail@juergen-riegel.net





set( ProjectSources
	Private/mainwindow.cpp
	Private/UndoCommandAdapter.cpp
	Private/UsdStageModel.cpp
	Private/SceneTreeView.cpp
	Private/CliServer.cpp
	Private/HydraViewerWindow.cpp
	Private/HydraViewportItem.cpp
	Private/HydraViewportRenderer.cpp
	Private/HydraAovSource.cpp
	Private/HydraCameraController.cpp
	Private/ViewerVulkanInstance.cpp
	#Private/viewitem.cpp
	#Private/scene.cpp
	Private/resources.qrc
)

set( ProjectHeaders
	Public/mainwindow.h
	Public/UndoCommandAdapter.h
	Public/UsdStageModel.h
	Public/SceneTreeView.h
	Public/CliServer.h
	Public/HydraViewerWindow.h
	Public/HydraViewportItem.h
	Public/HydraCameraController.h
	Private/HydraViewportRenderer.h
	Private/HydraAovSource.h
	Private/ViewerVulkanInstance.h
	#Public/viewitem.h
	#Public/scene.h
)



source_group ("Puplic Header" FILES
  ${ProjectHeaders}
)

source_group ("Source" FILES
  ${ProjectSources}
)


add_library( UsdStudioGuiLib   STATIC
	${ProjectSources}
	${ProjectHeaders} 
)

target_compile_definitions(UsdStudioGuiLib PUBLIC QT_NO_KEYWORDS)



set_property(TARGET UsdStudioGuiLib PROPERTY CXX_STANDARD 20)
if(WIN32)
	message("Building for Windows: disabling min/max macros from minwin.h which collide with OpenUSD max member function in robin_growth_policy.h")
	add_compile_definitions(NOMINMAX)
endif()


target_link_libraries (UsdStudioGuiLib 
	PRIVATE
	    UsdStudioAppLib
		Qt6::Core 
		Qt6::Gui 
		Qt6::GuiPrivate 
		Qt6::Widgets 
		Qt6::Network 
		Qt6::Quick 
		Qt6::QuickControls2
		Qt6::QuickWidgets
		Qt6::OpenGL
	PUBLIC
		UsdStudioOpenUSD
		UsdStudioImaging
)

# Compile the shaders and embed them in the static library itself.
set(shader_resource_dir "${CMAKE_CURRENT_BINARY_DIR}/hydra_shaders")
file(MAKE_DIRECTORY "${shader_resource_dir}")
foreach(shader IN ITEMS viewport.vert viewport.frag)
	set(shader_source "${CMAKE_CURRENT_SOURCE_DIR}/Shaders/${shader}")
	set(shader_output "${shader_resource_dir}/${shader}.qsb")
	add_custom_command(
		OUTPUT "${shader_output}"
		COMMAND Qt6::qsb --glsl "100es,120,150" --hlsl 50 --msl 12
			-o "${shader_output}" --depfile "${shader_output}.d" "${shader_source}"
		DEPENDS "${shader_source}" Qt6::qsb
		DEPFILE "${shader_output}.d"
		VERBATIM
	)
endforeach()
set(shader_qrc "${shader_resource_dir}/hydra_viewer_shaders.qrc")
file(CONFIGURE OUTPUT "${shader_qrc}" CONTENT [=[
<RCC><qresource prefix="/usdstudio">
<file alias="Shaders/viewport.vert.qsb">viewport.vert.qsb</file>
<file alias="Shaders/viewport.frag.qsb">viewport.frag.qsb</file>
</qresource></RCC>
]=])
qt_add_resources(shader_resources "${shader_qrc}")
target_sources(UsdStudioGuiLib PRIVATE ${shader_resources}
	Shaders/viewport.vert Shaders/viewport.frag)
set_source_files_properties(Shaders/viewport.vert Shaders/viewport.frag
	PROPERTIES HEADER_FILE_ONLY TRUE)

#target_include_directories (UsdStudioGuiLib PUBLIC	)

set_target_properties (UsdStudioGuiLib PROPERTIES PUBLIC_HEADER "${ProjectHeaders}")
set_target_properties (UsdStudioGuiLib PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/Public")

set_target_properties(UsdStudioGuiLib       PROPERTIES 
          ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
          LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
          RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
          )