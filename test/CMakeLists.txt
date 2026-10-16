# labels: unit -- headless, no GL; gui -- opens a (hidden) window, needs a display
function(smgk_add_test name srcfile label)
    add_executable(${name} ${srcfile})
    target_link_libraries(${name} PRIVATE smgk_core)
    target_include_directories(${name} PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_compile_definitions(${name} PRIVATE SMGK_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
    add_test(NAME ${name} COMMAND ${name})
    set_tests_properties(${name} PROPERTIES LABELS "${label}")
endfunction()

add_test(NAME view_starts COMMAND smgk-view --smg-hidden --smg-frames 2)
set_tests_properties(view_starts PROPERTIES LABELS gui)

foreach(t wire ansi feed)
    smgk_add_test(test_${t} test_${t}.cpp unit)
endforeach()
