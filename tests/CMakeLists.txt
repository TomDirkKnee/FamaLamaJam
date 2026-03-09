add_executable(famalamajam_tests
    unit/settings_validation_tests.cpp
    unit/state_serialization_tests.cpp
    unit/connection_lifecycle_tests.cpp
    unit/retry_policy_tests.cpp
    unit/connection_failure_classification_tests.cpp
    integration/plugin_apply_flow_tests.cpp
    integration/plugin_connection_command_flow_tests.cpp
    integration/plugin_connection_recovery_tests.cpp
    integration/plugin_state_roundtrip_tests.cpp)

target_link_libraries(famalamajam_tests
    PRIVATE
        famalamajam_core
        famalamajam_plugin
        Catch2::Catch2WithMain)

include(CTest)
include(Catch)
catch_discover_tests(famalamajam_tests
    TEST_PREFIX "phase::"
    PROPERTIES LABELS "phase-quick")
