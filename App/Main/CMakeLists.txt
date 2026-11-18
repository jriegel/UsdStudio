# (c)2024 Juergen Riegel, mail@juergen-riegel.net
#





add_executable( UsdStudioCmd  
	Main.cpp
)

#add_dependencies (UsdStudioCmd TKJT)

target_link_libraries (UsdStudioCmd PRIVATE 
								UsdStudioAppLib
						)
target_include_directories (UsdStudioCmd PUBLIC ${OpenCASCADE_INCLUDE_DIR} )


set_property(TARGET UsdStudioCmd PROPERTY CXX_STANDARD 20)
if(WIN32)
	add_compile_definitions(NOMINMAX)
endif()

set_target_properties(UsdStudioCmd       PROPERTIES 
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

if(MSVC)
	set_property(TARGET UsdStudioCmd PROPERTY LINK_FLAGS "/SUBSYSTEM:CONSOLE")
endif()

