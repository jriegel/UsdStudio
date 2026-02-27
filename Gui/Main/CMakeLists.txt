# (c)2024 Juergen Riegel, mail@juergen-riegel.net

set(MAIN_SRC
  main.cpp
  resource.rc
)
source_group("UsdStudio" FILES ${ENGVIEWER_SRC})


# Tell CMake to create the UsdStudio executable
add_executable(UsdStudio ${MAIN_SRC}  )

add_dependencies(UsdStudio 
	UsdStudioGuiLib
	Python3::InterpreterMultiConfig
)

set_property(TARGET UsdStudio PROPERTY CXX_STANDARD 20)
if(WIN32)
	message("Building for Windows: disabling min/max macros from minwin.h which collide with OpenUSD max member function in robin_growth_policy.h")
	add_compile_definitions(NOMINMAX)
endif()

target_link_libraries(UsdStudio 
		Qt::Widgets 
		Qt::QuickWidgets 
		UsdStudioGuiLib 
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

  set_target_properties(UsdStudio       PROPERTIES 
          ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
          LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
          RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
          )

