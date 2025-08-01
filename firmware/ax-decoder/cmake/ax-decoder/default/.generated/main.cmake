# cmake files support debug production
include("${CMAKE_CURRENT_LIST_DIR}/rule.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/file.cmake")

set(ax_decoder_default_library_list )

# Handle files with suffix (s|as|asm|AS|ASM|As|aS|Asm), for group default-XC8
if(ax_decoder_default_default_XC8_FILE_TYPE_assemble)
add_library(ax_decoder_default_default_XC8_assemble OBJECT ${ax_decoder_default_default_XC8_FILE_TYPE_assemble})
    ax_decoder_default_default_XC8_assemble_rule(ax_decoder_default_default_XC8_assemble)
    list(APPEND ax_decoder_default_library_list "$<TARGET_OBJECTS:ax_decoder_default_default_XC8_assemble>")
endif()

# Handle files with suffix S, for group default-XC8
if(ax_decoder_default_default_XC8_FILE_TYPE_assemblePreprocess)
add_library(ax_decoder_default_default_XC8_assemblePreprocess OBJECT ${ax_decoder_default_default_XC8_FILE_TYPE_assemblePreprocess})
    ax_decoder_default_default_XC8_assemblePreprocess_rule(ax_decoder_default_default_XC8_assemblePreprocess)
    list(APPEND ax_decoder_default_library_list "$<TARGET_OBJECTS:ax_decoder_default_default_XC8_assemblePreprocess>")
endif()

# Handle files with suffix [cC], for group default-XC8
if(ax_decoder_default_default_XC8_FILE_TYPE_compile)
add_library(ax_decoder_default_default_XC8_compile OBJECT ${ax_decoder_default_default_XC8_FILE_TYPE_compile})
    ax_decoder_default_default_XC8_compile_rule(ax_decoder_default_default_XC8_compile)
    list(APPEND ax_decoder_default_library_list "$<TARGET_OBJECTS:ax_decoder_default_default_XC8_compile>")
endif()


add_executable(${ax_decoder_default_image_name} ${ax_decoder_default_library_list})
set_target_properties(${ax_decoder_default_image_name} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${ax_decoder_default_output_dir})

target_link_libraries(${ax_decoder_default_image_name} PRIVATE ${ax_decoder_default_default_XC8_FILE_TYPE_link})

# Add the link options from the rule file.
ax_decoder_default_link_rule(${ax_decoder_default_image_name})




