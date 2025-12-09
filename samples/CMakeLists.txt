
function(add_engine_sample TARGET_NAME)
    add_executable(${TARGET_NAME} ${ARGN})
    target_link_libraries(${TARGET_NAME} PRIVATE EngineLib)

    if (CMAKE_VERSION VERSION_GREATER 3.12)
        set_property(TARGET ${TARGET_NAME} PROPERTY CXX_STANDARD 23)
    endif()

    # Copy config files
    file(GLOB CONFIG_FILES "${CMAKE_SOURCE_DIR}/config/*")
    if(CONFIG_FILES)
        add_custom_command(
            TARGET ${TARGET_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:${TARGET_NAME}>/config
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${CONFIG_FILES}
                $<TARGET_FILE_DIR:${TARGET_NAME}>/config/
            COMMENT "Copying config files to output directory"
        )
    endif()

    # Copy samples config files
    file(GLOB SAMPLES_CONFIG_FILES "${CMAKE_SOURCE_DIR}/samples/config/*")
    if(SAMPLES_CONFIG_FILES)
        add_custom_command(
            TARGET ${TARGET_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:${TARGET_NAME}>/config
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${SAMPLES_CONFIG_FILES}
                $<TARGET_FILE_DIR:${TARGET_NAME}>/config/
            COMMENT "Copying samples config files to output directory"
        )
    endif()

    # Copy assets
    if(ALL_KTX_TEXTURES)
        foreach(KTX_FILE ${ALL_KTX_TEXTURES})
            get_filename_component(KTX_NAME ${KTX_FILE} NAME)
            add_custom_command(
                TARGET ${TARGET_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:${TARGET_NAME}>/assets
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    ${KTX_FILE}
                    $<TARGET_FILE_DIR:${TARGET_NAME}>/assets/${KTX_NAME}
                COMMENT "Copying ${KTX_NAME} to sample output directory"
            )
        endforeach()
    endif()

    file(GLOB_RECURSE GLB_FILES "${CMAKE_SOURCE_DIR}/assets/*.glb")
    if(GLB_FILES)
        add_custom_command(
            TARGET ${TARGET_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:${TARGET_NAME}>/assets
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${GLB_FILES}
                $<TARGET_FILE_DIR:${TARGET_NAME}>/assets/
            COMMENT "Copying GLB files to sample output directory"
        )
    endif()

    # Copy samples assets
    file(GLOB_RECURSE SAMPLES_ASSET_FILES "${CMAKE_SOURCE_DIR}/samples/assets/*")
    if(SAMPLES_ASSET_FILES)
        add_custom_command(
            TARGET ${TARGET_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:${TARGET_NAME}>/assets
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${SAMPLES_ASSET_FILES}
                $<TARGET_FILE_DIR:${TARGET_NAME}>/assets/
            COMMENT "Copying samples asset files to sample output directory"
        )
    endif()
endfunction()

add_subdirectory(01_hello_world)
add_subdirectory(02_zoo)