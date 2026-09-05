find_package(Qt6 REQUIRED COMPONENTS Test Core Gui Widgets)

set(CMAKE_AUTOMOC ON)

# Test State Machine
add_executable(test_state_machine
    test_state_machine.cpp
    ../src/domain/StateMachine.cpp
)
target_include_directories(test_state_machine PRIVATE ../src)
target_link_libraries(test_state_machine PRIVATE Qt6::Core Qt6::Test)
add_test(NAME TestStateMachine COMMAND test_state_machine)

# Test Config Manager
add_executable(test_config_manager
    test_config_manager.cpp
    ../src/infrastructure/ConfigManager.cpp
)
target_include_directories(test_config_manager PRIVATE ../src)
target_link_libraries(test_config_manager PRIVATE Qt6::Core Qt6::Gui Qt6::Test)
add_test(NAME TestConfigManager COMMAND test_config_manager)

# Test Sleek Dialogs
add_executable(test_sleek_dialogs
    test_sleek_dialogs.cpp
    ../src/presentation/SleekDialogs.cpp
    ../src/presentation/GutterWidget.cpp
)
target_include_directories(test_sleek_dialogs PRIVATE ../src)
target_link_libraries(test_sleek_dialogs PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Test)
add_test(NAME TestSleekDialogs COMMAND test_sleek_dialogs)
