# (c)2024 Juergen Riegel, mail@juergen-riegel.net

set(MAIN_SRC
  main.cpp
)
if(WIN32)
  list(APPEND MAIN_SRC resource.rc)
endif()
source_group("UsdStudio" FILES ${ENGVIEWER_SRC})


# Tell CMake to create the UsdStudio executable
add_executable(UsdStudio ${MAIN_SRC}  )

add_dependencies(UsdStudio 
	UsdStudioGuiLib
	#Python3::InterpreterMultiConfig
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
		UsdStudioOpenUSD
) 

  set_target_properties(UsdStudio       PROPERTIES 
          ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
          LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
          RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
          )

if(WIN32)
  file(GLOB OPENUSD_PLUGIN_DLLS "${OPENUSD_ROOT}/lib/usd_*.dll")
  add_custom_command(TARGET UsdStudio POST_BUILD
    COMMAND $<TARGET_FILE:Qt6::windeployqt>
      $<IF:$<CONFIG:Debug>,--debug,--release>
      --no-translations --no-compiler-runtime
      --qmldir "${CMAKE_CURRENT_SOURCE_DIR}/../Src/Private"
      $<TARGET_FILE:UsdStudio>
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      $<TARGET_RUNTIME_DLLS:UsdStudio> $<TARGET_FILE_DIR:UsdStudio>
    COMMAND ${CMAKE_COMMAND} -E copy_directory
      "${OPENUSD_ROOT}/lib/usd" "$<TARGET_FILE_DIR:UsdStudio>/usd"
    # Scene-index/renderer plugin DLLs (hdGp etc.) that are not link-time
    # dependencies live in the OpenUSD lib dir; the plug system resolves
    # their LibraryPath relative to the copied plugin tree under usd/.
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      ${OPENUSD_PLUGIN_DLLS} "$<TARGET_FILE_DIR:UsdStudio>"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
      "${OPENUSD_ROOT}/plugin/usd" "$<TARGET_FILE_DIR:UsdStudio>/../plugin/usd"
    COMMAND_EXPAND_LISTS
    VERBATIM
  )
endif()
