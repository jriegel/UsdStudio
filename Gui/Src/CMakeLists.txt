# (c)2024 Juergen Riegel, mail@juergen-riegel.net





set( ProjectSources
	Private/TreeModel.cpp
	Private/TreeItem.cpp
	#Private/viewitem.cpp
	#Private/scene.cpp
	Private/ModelAdaptor.cpp
	Private/mainwindow.cpp
	Private/resources.qrc
)

set( ProjectHeaders
	Public/TreeModel.h
	Public/TreeItem.h
	#Public/viewitem.h
	#Public/scene.h
	Public/ModelAdaptor.h
	Public/mainwindow.h
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



set_property(TARGET UsdStudioGuiLib PROPERTY CXX_STANDARD 20)
if(WIN32)
	message("Building for Windows: disabling min/max macros from minwin.h which collide with OpenUSD max member function in robin_growth_policy.h")
	add_compile_definitions(NOMINMAX)
endif()


target_link_libraries (UsdStudioGuiLib 
	PRIVATE  
		Qt6::Core 
		Qt6::Widgets 
		Qt6::Network 
		Qt6::Quick 
		Qt6::QuickControls2
		Qt6::QuickWidgets
		Qt6::OpenGL
		ar
		arch
		gf
		js
		kind
		ndr
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

#target_include_directories (UsdStudioGuiLib PUBLIC	)

set_target_properties (UsdStudioGuiLib PROPERTIES PUBLIC_HEADER "${ProjectHeaders}")
set_target_properties (UsdStudioGuiLib PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/Public")

