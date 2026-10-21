function(reckon_add_test name)
    qt_add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE reckon_core Qt6::Test)
    add_test(NAME ${name} COMMAND ${name})
endfunction()

reckon_add_test(tst_engine)
reckon_add_test(tst_formatter)
reckon_add_test(tst_calculator)

qt_add_executable(tst_ui tst_ui.cpp)
target_link_libraries(tst_ui PRIVATE reckon_ui reckon_uiplugin Qt6::Test Qt6::QuickTest)
add_test(NAME tst_ui COMMAND tst_ui)
set_tests_properties(tst_ui PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QPA_PLATFORMTHEME=none")
