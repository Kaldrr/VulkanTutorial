find_package(Qt6 COMPONENTS 
		Widgets Core 3DExtras
REQUIRED)
find_package(Vulkan REQUIRED COMPONENTS glslc)
find_program(glslc_executable NAMES glslc HINTS Vulkan::glslc)
find_package(fmt CONFIG REQUIRED)

set (CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH};${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(HelperFunctions)

qt_standard_project_setup()

set(SOURCE_FILES 
	main.cpp
	MainWindow.cpp     
    VulkanRenderer.cpp 
    VulkanInstance.cpp 
    VulkanHelpers.cpp
    ModelManager.cpp
)
set(HEADER_FILES
    include/VulkanLoadingScreen/MainWindow.h
    include/VulkanLoadingScreen/VulkanRenderer.h
    include/VulkanLoadingScreen/VulkanInstance.h
    include/VulkanLoadingScreen/VulkanHelpers.h
    include/VulkanLoadingScreen/ModelManager.h
    include/VulkanLoadingScreen/Vertex.h
)
set(SHADER_FILES
    Shaders/shader.frag
    Shaders/shader.vert
)
set(MODEL_FILES
    Models/VikingRoom.obj
)
set(TEXTURE_FILES
    Textures/VikingRoom.png
)

qt_add_executable(VulkanLoadingScreen
        ${SOURCE_FILES}
        ${HEADER_FILES}
        ${SHADER_FILES}
)

target_link_libraries(VulkanLoadingScreen
	PRIVATE
		Qt6::Widgets
		Qt6::Core
        Qt6::3DExtras
        ${Vulkan_LIBRARIES}
        fmt::fmt
        assimp
)

target_include_directories(VulkanLoadingScreen
    PUBLIC
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

install(TARGETS VulkanLoadingScreen
    RUNTIME DESTINATION "${INSTALL_EXAMPLEDIR}"
    BUNDLE DESTINATION "${INSTALL_EXAMPLEDIR}"
    LIBRARY DESTINATION "${INSTALL_EXAMPLEDIR}"
)

if (WIN32)
    add_custom_command(TARGET VulkanLoadingScreen POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_RUNTIME_DLLS:VulkanLoadingScreen> $<TARGET_FILE_DIR:VulkanLoadingScreen>
      COMMAND_EXPAND_LISTS
    )
endif()

add_shader_dependency("${SHADER_FILES}")
add_model_dependency("${MODEL_FILES}")
add_textures_dependency("${TEXTURE_FILES}")

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
endif(WIN32)
