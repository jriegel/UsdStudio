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

# UsdStageModel: lazy chunking, filtering, path lookup, live refresh.
add_executable( UsdStageModelTests
	UsdStageModelTests.cpp
)

set_property(TARGET UsdStageModelTests PROPERTY CXX_STANDARD 20)

# Absolute path to the repository test data, normalized to forward slashes.
target_compile_definitions(UsdStageModelTests PRIVATE
	DATA_DIR="${CMAKE_SOURCE_DIR}/Data"
)

target_link_libraries(UsdStageModelTests PRIVATE
	UsdStudioGuiLib
	UsdStudioAppLib
	Qt6::Core
	Qt6::Gui
)

set_target_properties(UsdStageModelTests       PROPERTIES
          ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
          LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
          RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
          )

add_test(NAME UsdStageModelTests COMMAND UsdStageModelTests)

if(WIN32)
  add_custom_command(TARGET UsdStageModelTests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      $<TARGET_RUNTIME_DLLS:UsdStageModelTests> $<TARGET_FILE_DIR:UsdStageModelTests>
    COMMAND_EXPAND_LISTS
  )
endif()

# SceneTreeView / BreadcrumbBar: widget-level checks (needs a QApplication
# with the qwindows platform, so it runs on an interactive session only).
add_executable( SceneTreeViewTests
	SceneTreeViewTests.cpp
)

set_property(TARGET SceneTreeViewTests PROPERTY CXX_STANDARD 20)

target_link_libraries(SceneTreeViewTests PRIVATE
	UsdStudioGuiLib
	UsdStudioAppLib
	Qt6::Core
	Qt6::Gui
	Qt6::Widgets
)

set_target_properties(SceneTreeViewTests       PROPERTIES
          ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
          LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
          RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
          )

add_test(NAME SceneTreeViewTests COMMAND SceneTreeViewTests)

if(WIN32)
  add_custom_command(TARGET SceneTreeViewTests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      $<TARGET_RUNTIME_DLLS:SceneTreeViewTests> $<TARGET_FILE_DIR:SceneTreeViewTests>
    COMMAND_EXPAND_LISTS
  )
endif()

# CliServer: named-pipe remote control, request framing, shared undo history.
add_executable( CliServerTests
	CliServerTests.cpp
)

set_property(TARGET CliServerTests PROPERTY CXX_STANDARD 20)

target_link_libraries(CliServerTests PRIVATE
	UsdStudioGuiLib
	UsdStudioAppLib
	Qt6::Core
	Qt6::Gui
	Qt6::Network
)
add_dependencies(CliServerTests UsdStudioCmd)
target_compile_definitions(CliServerTests PRIVATE
	USDSTUDIO_CMD_PATH="$<TARGET_FILE:UsdStudioCmd>"
)

set_target_properties(CliServerTests       PROPERTIES
          ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
          LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
          RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
          )

add_test(NAME CliServerTests COMMAND CliServerTests)

if(WIN32)
  add_custom_command(TARGET CliServerTests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      $<TARGET_RUNTIME_DLLS:CliServerTests> $<TARGET_FILE_DIR:CliServerTests>
    COMMAND_EXPAND_LISTS
  )
endif()
