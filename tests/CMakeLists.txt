add_executable(famalamajam_tests
    unit/audio_buffer_resampler_tests.cpp
    unit/settings_validation_tests.cpp
    unit/state_serialization_tests.cpp
    unit/server_discovery_parser_tests.cpp
    unit/connection_lifecycle_tests.cpp
    unit/retry_policy_tests.cpp
    unit/connection_failure_classification_tests.cpp
    unit/codec_stream_bridge_tests.cpp
    unit/ogg_vorbis_codec_tests.cpp
    unit/remembered_server_store_tests.cpp
    # Phase 2 lifecycle recovery integration coverage
    integration/plugin_apply_flow_tests.cpp
    integration/plugin_connection_command_flow_tests.cpp
    integration/plugin_connection_recovery_tests.cpp
    integration/plugin_connection_error_recovery_tests.cpp
    integration/plugin_host_lifecycle_tests.cpp
    integration/plugin_state_roundtrip_tests.cpp
    integration/plugin_codec_runtime_tests.cpp
    integration/plugin_experimental_transport_tests.cpp
    integration/plugin_server_timing_sync_tests.cpp
    integration/plugin_host_sync_assist_tests.cpp
    integration/plugin_transport_ui_sync_tests.cpp
    integration/plugin_rehearsal_ui_flow_tests.cpp
    integration/plugin_room_controls_ui_tests.cpp
    integration/plugin_mixer_controls_tests.cpp
    integration/plugin_mixer_ui_tests.cpp
    integration/plugin_transmit_controls_tests.cpp
    integration/plugin_transmit_controls_ui_tests.cpp
    integration/plugin_voice_mode_guard_tests.cpp
    integration/plugin_voice_mode_transport_tests.cpp
    integration/plugin_pre_layout_cpu_diagnostics_tests.cpp
    integration/plugin_streaming_runtime_compat_tests.cpp
    integration/plugin_room_chat_tests.cpp
    integration/plugin_server_discovery_tests.cpp
    integration/plugin_server_discovery_ui_tests.cpp
    integration/plugin_remote_interval_alignment_tests.cpp)

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
