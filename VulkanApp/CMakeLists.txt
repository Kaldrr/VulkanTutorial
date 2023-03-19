find_package(Qt6 COMPONENTS 
		Widgets Core 3DExtras
REQUIRED)

qt_standard_project_setup()

set(SOURCE_FILES 
	main.cpp
	MainWindow.cpp     include/VulkanApp/MainWindow.h
    VulkanRenderer.cpp include/VulkanApp/VulkanRenderer.h
)

qt_add_executable(VulkanApp
    MANUAL_FINALIZATION
        ${SOURCE_FILES}
)

target_link_libraries(VulkanApp
	PRIVATE
		Qt6::Widgets
		Qt6::Core
        Qt6::3DExtras
)

target_include_directories(VulkanApp
    PRIVATE
        include
)

set_property(
	TARGET 
		VulkanApp 
	PROPERTY 
		CXX_STANDARD 23
)

target_compile_options(VulkanApp PRIVATE ${WERROR_FLAG} ${PEDANTIC_COMPILE_FLAGS})

# Resources:
set_source_files_properties("Shaders/color_frag.spv"
    PROPERTIES QT_RESOURCE_ALIAS "color_frag.spv"
)

set_source_files_properties("Shaders/color_vert.spv"
    PROPERTIES QT_RESOURCE_ALIAS "color_vert.spv"
)

set(vulkanappshaders_resource_files
    "Shaders/color_frag.spv"
    "Shaders/color_vert.spv"
)

qt_add_resources(VulkanApp "VulkanApp"
    PREFIX
        "/"
    FILES
        ${vulkanappshaders_resource_files}
)

install(TARGETS VulkanApp
    RUNTIME DESTINATION "${INSTALL_EXAMPLEDIR}"
    BUNDLE DESTINATION "${INSTALL_EXAMPLEDIR}"
    LIBRARY DESTINATION "${INSTALL_EXAMPLEDIR}"
)

qt_finalize_executable(VulkanApp)

add_custom_command(TARGET VulkanApp POST_BUILD
  COMMAND ${CMAKE_COMMAND} $<$<CXX_COMPILER_ID:MSVC>:-E> copy $<TARGET_RUNTIME_DLLS:VulkanApp> $<TARGET_FILE_DIR:VulkanApp>
  COMMAND_EXPAND_LISTS
)

if (WIN32) 
    get_target_property(_qmake_executable Qt6::qmake IMPORTED_LOCATION)
    get_filename_component(_qt_bin_dir "${_qmake_executable}" DIRECTORY)
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${_qt_bin_dir}")

    add_custom_command(TARGET VulkanApp POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E
            env PATH="${_qt_bin_dir}" "${WINDEPLOYQT_EXECUTABLE}"
                "$<$<CONFIG:Debug>:--debug>"
                "$<$<CONFIG:Release>:--release>"
                "--no-compiler-runtime"
                "$<TARGET_FILE:VulkanApp>"
        COMMENT "Running windeployqt..."
)
endif(WIN32)
