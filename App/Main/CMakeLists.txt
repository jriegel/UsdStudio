# (c)2024 Juergen Riegel, mail@juergen-riegel.net
#





add_executable( UsdStudioCmd  
	Main.cpp
)

#add_dependencies (UsdStudioCmd TKJT)

target_link_libraries (UsdStudioCmd PRIVATE 							
								CLI11::CLI11
						)
target_include_directories (UsdStudioCmd PUBLIC ${OpenCASCADE_INCLUDE_DIR} )


