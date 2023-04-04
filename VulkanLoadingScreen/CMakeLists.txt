find_package(Qt6 COMPONENTS 
		Widgets Core 3DExtras
REQUIRED)
find_package(Vulkan REQUIRED COMPONENTS glslc)
find_program(glslc_executable NAMES glslc HINTS Vulkan::glslc)
find_package(fmt CONFIG REQUIRED)

qt_standard_project_setup()

set(SOURCE_FILES 
	main.cpp
	MainWindow.cpp     
    VulkanRenderer.cpp 
    VulkanInstance.cpp 
    VulkanHelpers.cpp
)
set(HEADER_FILES
    include/VulkanLoadingScreen/MainWindow.h
    include/VulkanLoadingScreen/VulkanRenderer.h
    include/VulkanLoadingScreen/VulkanInstance.h
    include/VulkanLoadingScreen/VulkanHelpers.h
)
set(SHADER_FILES
    Shaders/shader.frag
    Shaders/shader.vert
)

qt_add_executable(VulkanLoadingScreen
    MANUAL_FINALIZATION
        ${SOURCE_FILES}
        ${HEADER_FILES}
        ${SHADER_FILES}
)

target_link_libraries(VulkanLoadingScreen
	PRIVATE
		Qt6::Widgets
		Qt6::Core
        Qt6::3DExtras
        ${VULKAN_LIB_LIST}
        fmt::fmt
)

target_include_directories(VulkanLoadingScreen
    PRIVATE
        include
        ${VULKAN_INCLUDE_DIRS}
)

target_compile_definitions(VulkanLoadingScreen
    PRIVATE
        VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1
)

set_property(
	TARGET 
		VulkanLoadingScreen 
	PROPERTY 
		CXX_STANDARD 23
)

target_compile_options(VulkanLoadingScreen PRIVATE ${WERROR_FLAG} ${PEDANTIC_COMPILE_FLAGS})

# Resources:
#set_source_files_properties("Shaders/color_frag.spv"
#    PROPERTIES QT_RESOURCE_ALIAS "color_frag.spv"
#)

#set_source_files_properties("Shaders/color_vert.spv"
#    PROPERTIES QT_RESOURCE_ALIAS "color_vert.spv"
#)

#set(VulkanLoadingScreen_resource_files
#    "Shaders/color_frag.spv"
#    "Shaders/color_vert.spv"
#)

#qt_add_resources(VulkanLoadingScreen "VulkanLoadingScreen"
#    PREFIX
#        "/"
#    FILES
#        ${VulkanLoadingScreen_resource_files}
#)

install(TARGETS VulkanLoadingScreen
    RUNTIME DESTINATION "${INSTALL_EXAMPLEDIR}"
    BUNDLE DESTINATION "${INSTALL_EXAMPLEDIR}"
    LIBRARY DESTINATION "${INSTALL_EXAMPLEDIR}"
)

qt_finalize_executable(VulkanLoadingScreen)

add_custom_command(TARGET VulkanLoadingScreen POST_BUILD
  COMMAND ${CMAKE_COMMAND} $<$<CXX_COMPILER_ID:MSVC>:-E> copy $<TARGET_RUNTIME_DLLS:VulkanLoadingScreen> $<TARGET_FILE_DIR:VulkanLoadingScreen>
  COMMAND_EXPAND_LISTS
)

if (WIN32) 
    get_target_property(_qmake_executable Qt6::qmake IMPORTED_LOCATION)
    get_filename_component(_qt_bin_dir "${_qmake_executable}" DIRECTORY)
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${_qt_bin_dir}")

    add_custom_command(TARGET VulkanLoadingScreen POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E
            env PATH="${_qt_bin_dir}" "${WINDEPLOYQT_EXECUTABLE}"
                "$<$<CONFIG:Debug>:--debug>"
                "$<$<CONFIG:Release>:--release>"
                "--no-compiler-runtime"
                "$<TARGET_FILE:VulkanLoadingScreen>"
        COMMENT "Running windeployqt..."
)
    add_custom_command(TARGET VulkanLoadingScreen PRE_BUILD COMMAND ${CMAKE_COMMAND} -E make_directory "Shaders")

    # TODO Change it into a function
    add_custom_command(TARGET VulkanLoadingScreen PRE_BUILD
        COMMAND
            ${glslc_executable} "--target-env=vulkan1.3"
            "${CMAKE_CURRENT_SOURCE_DIR}/Shaders/shader.vert" "-o" "Shaders/vert.spv"
    )
    add_custom_command(TARGET VulkanLoadingScreen PRE_BUILD
        COMMAND
            ${glslc_executable} "--target-env=vulkan1.3"
            "${CMAKE_CURRENT_SOURCE_DIR}/Shaders/shader.frag" "-O" "-o" "Shaders/frag.spv"
    )

    # we don't want to copy if we're building in the source dir
if (NOT CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_CURRENT_BINARY_DIR)

    # list of files for which we add a copy rule
    set(resource_files "texture.jpg")

    add_custom_command(
        TARGET VulkanLoadingScreen 
        PRE_BUILD 
        COMMAND ${CMAKE_COMMAND} -E make_directory "Textures"
    )
    foreach(item IN LISTS resource_files)
        add_custom_command(
            TARGET VulkanLoadingScreen PRE_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/Textures/${item}" "${CMAKE_CURRENT_BINARY_DIR}/Textures/${item}"
            DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Textures/${item}"
        )
    endforeach()
endif()

endif(WIN32)
