find_package(assimp CONFIG REQUIRED)
find_package(
  Qt6
  COMPONENTS Widgets Core 3DExtras
  REQUIRED)
find_package(Vulkan REQUIRED COMPONENTS glslc)
find_program(
  glslc_executable
  NAMES glslc
  HINTS Vulkan::glslc)
find_package(fmt CONFIG REQUIRED)

set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH};${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(HelperFunctions)

qt_standard_project_setup()

set(SOURCE_FILES main.cpp MainWindow.cpp VulkanRenderer.cpp VulkanInstance.cpp
                 VulkanHelpers.cpp ModelManager.cpp)
set(HEADER_FILES
    include/VulkanTutorial/MainWindow.h
    include/VulkanTutorial/VulkanRenderer.h
    include/VulkanTutorial/VulkanInstance.h
    include/VulkanTutorial/VulkanHelpers.h
    include/VulkanTutorial/ModelManager.h
    include/VulkanTutorial/Vertex.h)
set(SHADER_FILES Shaders/shader.frag Shaders/shader.vert)
set(MODEL_FILES Models/VikingRoom.obj)
set(TEXTURE_FILES Textures/VikingRoom.png)

qt_add_executable(VulkanTutorial ${SOURCE_FILES} ${HEADER_FILES})

target_link_libraries(
  VulkanTutorial
  PRIVATE Qt6::Widgets
          Qt6::Core
          Qt6::3DExtras
          ${Vulkan_LIBRARIES}
          fmt::fmt
          assimp::assimp
          VulkanTutorial_project_options
          VulkanTutorial_project_warnings)

target_include_directories(VulkanTutorial PUBLIC include ${VULKAN_INCLUDE_DIRS})

target_compile_definitions(
  VulkanTutorial PRIVATE VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1
                         VULKAN_HPP_NO_CONSTRUCTORS)

if(LINUX)
  target_compile_definitions(VulkanTutorial PRIVATE VK_USE_PLATFORM_WAYLAND_KHR
                                                    VK_USE_PLATFORM_XCB_KHR)
elseif(WIN32)
  target_compile_definitions(VulkanTutorial PRIVATE VK_USE_PLATFORM_WIN32_KHR)
endif()

set_target_properties(VulkanTutorial PROPERTIES WIN32_EXECUTABLE ON
                                                MACOSX_BUNDLE ON)

set_property(TARGET VulkanTutorial PROPERTY CXX_STANDARD 23)

install(
  TARGETS VulkanTutorial
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  BUNDLE DESTINATION .
  LIBRARY DESTINATION ${CMAKE_INSTALL_BINDIR})

if(WIN32)
  add_custom_command(
    TARGET VulkanTutorial
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_RUNTIME_DLLS:VulkanTutorial>
            $<TARGET_FILE_DIR:VulkanTutorial>
    COMMAND_EXPAND_LISTS)

  get_target_property(_qmake_executable Qt6::qmake IMPORTED_LOCATION)
  get_filename_component(_qt_bin_dir "${_qmake_executable}" DIRECTORY)
  find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${_qt_bin_dir}")

  add_custom_command(
    TARGET VulkanTutorial
    POST_BUILD
    COMMAND
      "${WINDEPLOYQT_EXECUTABLE}" "$<$<CONFIG:Debug>:--debug>"
      "$<$<CONFIG:Release>:--release>" "--no-compiler-runtime"
      "$<TARGET_FILE:VulkanTutorial>" "--no-translations"
    COMMENT "Running windeployqt...")
endif(WIN32)

add_shader_dependency("${SHADER_FILES}")
add_model_dependency("${MODEL_FILES}")
add_textures_dependency("${TEXTURE_FILES}")
