# (c)2024 Juergen Riegel, mail@juergen-riegel.net

add_executable( UsdStudioAppTests
	CommandTests.cpp
)

set_property(TARGET UsdStudioAppTests PROPERTY CXX_STANDARD 20)
if(WIN32)
	add_compile_definitions(NOMINMAX)
endif()

target_link_libraries(UsdStudioAppTests PRIVATE
	UsdStudioAppLib
)

# Absolute path to the repository test data, normalized to forward slashes.
target_compile_definitions(UsdStudioAppTests PRIVATE
	DATA_DIR="${CMAKE_SOURCE_DIR}/Data"
)

set_target_properties(UsdStudioAppTests       PROPERTIES
          ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
          LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
          RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
          )

add_test(NAME UsdStudioAppTests COMMAND UsdStudioAppTests)

if(WIN32)
  # Put the OpenUSD DLLs next to the test exe so ctest can run it.
  add_custom_command(TARGET UsdStudioAppTests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      $<TARGET_RUNTIME_DLLS:UsdStudioAppTests> $<TARGET_FILE_DIR:UsdStudioAppTests>
    COMMAND_EXPAND_LISTS
  )
endif()
