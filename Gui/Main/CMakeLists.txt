# (c)2018 Juergen Riegel, Daimler Protics GmbH, Juergen.Riegel@daimler.com
# Instruct CMake to run moc automatically when needed.




set(MAIN_SRC
  main.cpp
  resource.rc
)
source_group("UsdStudio" FILES ${ENGVIEWER_SRC})


# Tell CMake to create the UsdStudio executable
add_executable(UsdStudio WIN32 ${MAIN_SRC}  )

add_dependencies(UsdStudio UsdStudioGuiLib)

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


add_custom_command(TARGET UsdStudio
	COMMAND ${WINDEPLOYQT_EXECUTABLE} "$<TARGET_FILE_DIR:UsdStudio>/$<TARGET_FILE_NAME:UsdStudio>" 

#	POST_BUILD
#	#### Copy the required plugins to the binary directory. ####
#	#### Mandatory for the application to run. ####
#	COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/lib/usd/"
#	
#	# Easy solution: copy all directories from "/lib/usd" of OpenUSD installed
#	# by vcpkg to the "/lib/usd" of your application.
#	COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different  "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows/lib/usd" "${CMAKE_BINARY_DIR}/lib/usd"
)