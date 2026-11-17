# (c)2024 Juergen Riegel, mail@juergen-riegel.net

add_executable( UsdStudioGuiTests
	UndoAdapterTests.cpp
)

set_property(TARGET UsdStudioGuiTests PROPERTY CXX_STANDARD 20)
if(WIN32)
	add_compile_definitions(NOMINMAX)
endif()

target_link_libraries(UsdStudioGuiTests PRIVATE
	UsdStudioGuiLib
	UsdStudioAppLib
	Qt6::Core
	Qt6::Gui
)

set_target_properties(UsdStudioGuiTests       PROPERTIES
          ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
          LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
          RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
          )

add_test(NAME UsdStudioGuiTests COMMAND UsdStudioGuiTests)

if(WIN32)
  # Put the Qt and OpenUSD DLLs next to the test exe so ctest can run it.
  add_custom_command(TARGET UsdStudioGuiTests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      $<TARGET_RUNTIME_DLLS:UsdStudioGuiTests> $<TARGET_FILE_DIR:UsdStudioGuiTests>
    COMMAND_EXPAND_LISTS
  )
endif()
