function (add_shader_dependency shaders)
	add_custom_command(
		TARGET VulkanLoadingScreen 
		PRE_BUILD COMMAND 
		${CMAKE_COMMAND} -E make_directory "Shaders"
	)

	foreach(shader ${shaders})
		get_filename_component(FILENAME ${shader} NAME_WLE)
		get_filename_component(EXTENSION ${shader} EXT)
		# Remove the '.', ".frag" => "frag"
		string(SUBSTRING "${EXTENSION}" 1 -1 EXTENSION)
		set(OUT_SHADER_FILE "${CMAKE_CURRENT_BINARY_DIR}/Shaders/${EXTENSION}.spv")
		add_custom_command(
			OUTPUT "${OUT_SHADER_FILE}"
			COMMAND ${glslc_executable} "--target-env=vulkan1.3" "${CMAKE_CURRENT_SOURCE_DIR}/${shader}" "-O" "-o" "${OUT_SHADER_FILE}"
			MAIN_DEPENDENCY "${CMAKE_CURRENT_SOURCE_DIR}/${shader}"
		)
		list(APPEND SPV_SHADERS "${OUT_SHADER_FILE}")
	endforeach()
	add_custom_target(shaders ALL DEPENDS "${SPV_SHADERS}")
	add_dependencies(VulkanLoadingScreen shaders)
endfunction()

function (add_model_dependency models)
	add_custom_command(
		TARGET VulkanLoadingScreen 
		PRE_BUILD COMMAND 
                ${CMAKE_COMMAND} -E make_directory "Models"
	)
        foreach(model ${models})
        add_custom_command(
            OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${model}"
            COMMAND ${CMAKE_COMMAND} -E copy
                    ${CMAKE_CURRENT_SOURCE_DIR}/${model}
                    ${CMAKE_CURRENT_BINARY_DIR}/${model}
            DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${model}"
        )
		list(APPEND MODEL_TARGETS "${CMAKE_CURRENT_BINARY_DIR}/${model}")
	endforeach()
	add_custom_target(models ALL DEPENDS "${MODEL_TARGETS}")
	add_dependencies(VulkanLoadingScreen models)
endfunction()

function (add_textures_dependency textures)
	add_custom_command(
		TARGET VulkanLoadingScreen 
		PRE_BUILD COMMAND 
                ${CMAKE_COMMAND} -E make_directory "Textures"
	)
	foreach(texture ${textures})
        add_custom_command(
            OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${texture}"
            COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/${texture}" "${CMAKE_CURRENT_BINARY_DIR}/${texture}"
            DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${texture}"
        )
		list(APPEND TEXTURE_TARGETS "${CMAKE_CURRENT_BINARY_DIR}/${texture}")
	endforeach()
	add_custom_target(textures ALL DEPENDS "${TEXTURE_TARGETS}")
	add_dependencies(VulkanLoadingScreen textures)
endfunction()
