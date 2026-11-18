# (c)2024 Juergen Riegel, mail@juergen-riegel.net





set( ProjectSources
	Private/Document.cpp
	Private/Commands/CommandRegistry.cpp
	Private/Commands/CommandHistory.cpp
	Private/Commands/DocumentCommands.cpp
	Private/Commands/QueryRegistry.cpp
	Private/Commands/StageQueries.cpp
	Private/Commands/StageEditCommands.cpp
	Private/Commands/AppCommands.cpp
	Private/Cli/TypedValue.cpp
	Private/Cli/Cli.cpp
	
)

set( ProjectHeaders
	Public/Document.h
	Public/AppContext.h
	Public/Commands/ICommand.h
	Public/Commands/CommandRegistry.h
	Public/Commands/CommandHistory.h
	Public/Commands/CommandArgSpec.h
	Public/Commands/CommandError.h
	Public/Commands/DocumentCommands.h
	Public/Commands/QueryRegistry.h
	Public/Commands/StageQueries.h
	Public/Commands/StageEditCommands.h
	Public/Commands/AppCommands.h
	Public/Cli/Json.h
	Public/Cli/TypedValue.h
	Public/Cli/Cli.h
	
)



source_group ("Puplic Header" FILES
  ${ProjectHeaders}
)

source_group ("Source" FILES
  ${ProjectSources}
)


add_library( UsdStudioAppLib   STATIC
	${ProjectSources}
	${ProjectHeaders} 
)



set_property(TARGET UsdStudioAppLib PROPERTY CXX_STANDARD 20)
if(WIN32)
	message("Building for Windows: disabling min/max macros from minwin.h which collide with OpenUSD max member function in robin_growth_policy.h")
	add_compile_definitions(NOMINMAX)
endif()


target_link_libraries (UsdStudioAppLib 
	PUBLIC
		UsdStudioOpenUSD
)

#target_include_directories (UsdStudioAppLib PUBLIC	)

set_target_properties (UsdStudioAppLib PROPERTIES PUBLIC_HEADER "${ProjectHeaders}")
set_target_properties (UsdStudioAppLib PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/Public")

set_target_properties(UsdStudioAppLib       PROPERTIES 
          ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
          LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
          RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
          )