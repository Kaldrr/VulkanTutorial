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
    include/VulkanLoadingScreen/MainWindow.h
    include/VulkanLoadingScreen/VulkanRenderer.h
    include/VulkanLoadingScreen/VulkanInstance.h
    include/VulkanLoadingScreen/VulkanHelpers.h
    include/VulkanLoadingScreen/ModelManager.h
    include/VulkanLoadingScreen/Vertex.h)
set(SHADER_FILES Shaders/shader.frag Shaders/shader.vert)
set(MODEL_FILES Models/VikingRoom.obj)
set(TEXTURE_FILES Textures/VikingRoom.png)

qt_add_executable(VulkanLoadingScreen ${SOURCE_FILES} ${HEADER_FILES})

target_link_libraries(
  VulkanLoadingScreen
  PRIVATE Qt6::Widgets
          Qt6::Core
          Qt6::3DExtras
          ${Vulkan_LIBRARIES}
          fmt::fmt
          assimp::assimp
          VulkanLoadingScreen_project_options
          VulkanLoadingScreen_project_warnings)

target_include_directories(VulkanLoadingScreen PUBLIC include
                                                      ${VULKAN_INCLUDE_DIRS})

target_compile_definitions(VulkanLoadingScreen
                           PRIVATE VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1)

set_target_properties(VulkanLoadingScreen PROPERTIES WIN32_EXECUTABLE ON
                                                     MACOSX_BUNDLE ON)

set_property(TARGET VulkanLoadingScreen PROPERTY CXX_STANDARD 23)

install(
  TARGETS VulkanLoadingScreen
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  BUNDLE DESTINATION .
  LIBRARY DESTINATION ${CMAKE_INSTALL_BINDIR})

if(WIN32)
  add_custom_command(
    TARGET VulkanLoadingScreen
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_RUNTIME_DLLS:VulkanLoadingScreen>
            $<TARGET_FILE_DIR:VulkanLoadingScreen>
    COMMAND_EXPAND_LISTS)

  get_target_property(_qmake_executable Qt6::qmake IMPORTED_LOCATION)
  get_filename_component(_qt_bin_dir "${_qmake_executable}" DIRECTORY)
  find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${_qt_bin_dir}")

  add_custom_command(
    TARGET VulkanLoadingScreen
    POST_BUILD
    COMMAND
      "${WINDEPLOYQT_EXECUTABLE}" "$<$<CONFIG:Debug>:--debug>"
      "$<$<CONFIG:Release>:--release>" "--no-compiler-runtime"
      "$<TARGET_FILE:VulkanLoadingScreen>" "--no-translations"
    COMMENT "Running windeployqt...")
endif(WIN32)

add_shader_dependency("${SHADER_FILES}")
add_model_dependency("${MODEL_FILES}")
add_textures_dependency("${TEXTURE_FILES}")
