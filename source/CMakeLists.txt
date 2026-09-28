add_executable(${ProjectName}
    RainbowPico.c
    Hardware.c
    ComponentControl.c
)

pico_set_program_name(${ProjectName} "RainbowPico")
pico_set_program_version(${ProjectName} "0.1")

target_include_directories(${ProjectName} PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
)

target_compile_options(${ProjectName} PRIVATE
    $<$<COMPILE_LANG_AND_ID:C,Clang,GNU>:-Wall>
    $<$<COMPILE_LANG_AND_ID:C,Clang,GNU>:-Wextra>
    $<$<COMPILE_LANG_AND_ID:C,Clang,GNU>:-Wpedantic>
)

target_link_libraries(${ProjectName}
    pico_stdlib
    hardware_pwm
    FreeRTOS-Kernel-Heap4
)

pico_enable_stdio_usb(${ProjectName} 1)
pico_enable_stdio_uart(${ProjectName} 0)

pico_add_extra_outputs(${ProjectName})
