# (c)2024 Juergen Riegel, mail@juergen-riegel.net
#





add_executable( UsdStudioCmd  
	Main.cpp
)

#add_dependencies (UsdStudioCmd TKJT)

target_link_libraries (UsdStudioCmd PRIVATE 
								UsdStudioAppLib
								CLI11::CLI11
						)
target_include_directories (UsdStudioCmd PUBLIC ${OpenCASCADE_INCLUDE_DIR} )


set_target_properties(UsdStudioCmd       PROPERTIES 
		LINK_FLAGS "/SUBSYSTEM:CONSOLE"
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

