#include <string>

#include <catch2/catch_test_macros.hpp>

#include "app/session/ConnectionLifecycle.h"
#include "plugin/FamaLamaJamAudioProcessor.h"
#include "support/MiniNinjamServer.h"

namespace
{
using famalamajam::app::session::ConnectionEvent;
using famalamajam::app::session::ConnectionEventType;
using famalamajam::app::session::ConnectionState;
using famalamajam::plugin::FamaLamaJamAudioProcessor;
using famalamajam::tests::integration::MESSAGE_CHAT_MESSAGE;
using famalamajam::tests::integration::MiniNinjamServer;
using famalamajam::tests::integration::fillRampBuffer;

void connectProcessor(FamaLamaJamAudioProcessor& processor, MiniNinjamServer& server)
{
    auto settings = processor.getActiveSettings();
    settings.serverHost = "127.0.0.1";
    settings.serverPort = static_cast<std::uint16_t>(server.port());
    settings.username = "guest";

    REQUIRE(processor.applySettingsFromUi(settings));
    processor.prepareToPlay(48000.0, 512);
    REQUIRE(processor.requestConnect());
    REQUIRE(server.waitForAuthentication(2000));
    REQUIRE(processor.getLifecycleSnapshot().state == ConnectionState::Active);
}

void pumpTransport(FamaLamaJamAudioProcessor& processor,
                   juce::AudioBuffer<float>& buffer,
                   juce::MidiBuffer& midi,
                   int iterations = 64)
{
    for (int iteration = 0; iteration < iterations; ++iteration)
    {
        fillRampBuffer(buffer);
        processor.processBlock(buffer, midi);
        juce::Thread::sleep(5);
    }
}

bool waitForFeedSize(FamaLamaJamAudioProcessor& processor,
                     juce::AudioBuffer<float>& buffer,
                     juce::MidiBuffer& midi,
                     std::size_t expectedSize,
                     int maxAttempts = 120)
{
    for (int attempt = 0; attempt < maxAttempts; ++attempt)
    {
        pumpTransport(processor, buffer, midi, 1);
        if (processor.getRoomUiState().visibleFeed.size() >= expectedSize)
            return true;
    }

    return false;
}
} // namespace

TEST_CASE("plugin room feed preserves inbound room events in arrival order", "[plugin_room_chat]")
{
    MiniNinjamServer server;
    REQUIRE(server.startServer());

    FamaLamaJamAudioProcessor processor(true, true);
    connectProcessor(processor, server);

    juce::AudioBuffer<float> buffer(2, 512);
    juce::MidiBuffer midi;

    server.enqueueRoomChatMessage("alice", "Hello room");
    server.enqueueRoomTopic("host", "Warm-up in A minor");
    server.enqueueRoomJoin("bob");
    server.enqueueRoomSystemMessage("[voting system] leading candidate: 1/2 votes for 120 BPM [each vote expires in 60s]");
    server.enqueueRoomPart("carol");
    server.enqueueRoomSystemMessage("Server will restart soon");

    REQUIRE(waitForFeedSize(processor, buffer, midi, 6));

    const auto roomState = processor.getRoomUiState();
    REQUIRE(roomState.connected);
    REQUIRE(roomState.topic == "Warm-up in A minor");
    REQUIRE(roomState.visibleFeed.size() >= 6);

    CHECK(roomState.visibleFeed[0].kind == FamaLamaJamAudioProcessor::RoomFeedEntryKind::Chat);
    CHECK(roomState.visibleFeed[0].author == "alice");
    CHECK(roomState.visibleFeed[0].text == "Hello room");

    CHECK(roomState.visibleFeed[1].kind == FamaLamaJamAudioProcessor::RoomFeedEntryKind::Topic);
    CHECK(roomState.visibleFeed[1].text == "Warm-up in A minor");

    CHECK(roomState.visibleFeed[2].kind == FamaLamaJamAudioProcessor::RoomFeedEntryKind::Presence);
    CHECK(roomState.visibleFeed[2].author == "bob");
    CHECK(roomState.visibleFeed[2].subdued);

    CHECK(roomState.visibleFeed[3].kind == FamaLamaJamAudioProcessor::RoomFeedEntryKind::VoteSystem);
    CHECK(roomState.visibleFeed[3].author.empty());
    CHECK(roomState.visibleFeed[3].text.find("[voting system]") != std::string::npos);

    CHECK(roomState.visibleFeed[4].kind == FamaLamaJamAudioProcessor::RoomFeedEntryKind::Presence);
    CHECK(roomState.visibleFeed[4].author == "carol");
    CHECK(roomState.visibleFeed[4].subdued);

    CHECK(roomState.visibleFeed[5].kind == FamaLamaJamAudioProcessor::RoomFeedEntryKind::GenericSystem);
    CHECK(roomState.visibleFeed[5].author.empty());
    CHECK(roomState.visibleFeed[5].text == "Server will restart soon");
}

TEST_CASE("plugin room topic updates pinned topic and feed entries", "[plugin_room_chat]")
{
    MiniNinjamServer server;
    REQUIRE(server.startServer());

    FamaLamaJamAudioProcessor processor(true, true);
    connectProcessor(processor, server);

    juce::AudioBuffer<float> buffer(2, 512);
    juce::MidiBuffer midi;

    server.enqueueRoomTopic({}, "Topic is: intro only");
    server.enqueueRoomTopic("host", "Switch to chorus rehearsal");

    REQUIRE(waitForFeedSize(processor, buffer, midi, 2));

    const auto roomState = processor.getRoomUiState();
    REQUIRE(roomState.topic == "Switch to chorus rehearsal");
    REQUIRE(roomState.visibleFeed.size() >= 2);

    CHECK(roomState.visibleFeed[0].kind == FamaLamaJamAudioProcessor::RoomFeedEntryKind::Topic);
    CHECK(roomState.visibleFeed[0].text == "Topic is: intro only");
    CHECK(roomState.visibleFeed[1].kind == FamaLamaJamAudioProcessor::RoomFeedEntryKind::Topic);
    CHECK(roomState.visibleFeed[1].author == "host");
    CHECK(roomState.visibleFeed[1].text == "Switch to chorus rehearsal");
}

TEST_CASE("plugin room commands emit MESSAGE_CHAT_MESSAGE payloads", "[plugin_room_chat]")
{
    MiniNinjamServer server;
    REQUIRE(server.startServer());

    FamaLamaJamAudioProcessor processor(true, true);
    connectProcessor(processor, server);

    REQUIRE(processor.sendRoomChatMessage("Ready to jam"));
    REQUIRE(processor.submitRoomVote(FamaLamaJamAudioProcessor::RoomVoteKind::Bpm, 123));
    REQUIRE(processor.submitRoomVote(FamaLamaJamAudioProcessor::RoomVoteKind::Bpi, 12));

    MiniNinjamServer::RoomMessage outbound;
    REQUIRE(server.waitForCapturedRoomMessage(2000, outbound));
    CHECK(outbound.fields[0] == "MSG");
    CHECK(outbound.fields[1] == "Ready to jam");

    REQUIRE(server.waitForCapturedRoomMessage(2000, outbound));
    CHECK(outbound.fields[0] == "MSG");
    CHECK(outbound.fields[1] == "!vote bpm 123");

    REQUIRE(server.waitForCapturedRoomMessage(2000, outbound));
    CHECK(outbound.fields[0] == "MSG");
    CHECK(outbound.fields[1] == "!vote bpi 12");

    CHECK(MESSAGE_CHAT_MESSAGE == 0xC0);
}

TEST_CASE("plugin room state clears on disconnect and new live session authentication", "[plugin_room_chat]")
{
    MiniNinjamServer firstServer;
    REQUIRE(firstServer.startServer());

    FamaLamaJamAudioProcessor processor(true, true);
    connectProcessor(processor, firstServer);

    juce::AudioBuffer<float> buffer(2, 512);
    juce::MidiBuffer midi;

    firstServer.enqueueRoomChatMessage("alice", "Hello room");
    firstServer.enqueueRoomTopic("host", "Warm-up in A minor");
    REQUIRE(waitForFeedSize(processor, buffer, midi, 2));

    REQUIRE(processor.submitRoomVote(FamaLamaJamAudioProcessor::RoomVoteKind::Bpm, 130));
    {
        const auto roomState = processor.getRoomUiState();
        CHECK(roomState.bpmVote.pending);
        CHECK(roomState.bpmVote.requestedValue == 130);
    }

    REQUIRE(processor.requestDisconnect());
    {
        const auto roomState = processor.getRoomUiState();
        CHECK_FALSE(roomState.connected);
        CHECK(roomState.topic.empty());
        CHECK(roomState.visibleFeed.empty());
        CHECK_FALSE(roomState.bpmVote.pending);
        CHECK_FALSE(roomState.bpiVote.pending);
    }

    MiniNinjamServer secondServer;
    REQUIRE(secondServer.startServer());

    auto settings = processor.getActiveSettings();
    settings.serverPort = static_cast<std::uint16_t>(secondServer.port());
    REQUIRE(processor.applySettingsFromUi(settings));
    REQUIRE(processor.requestConnect());
    REQUIRE(secondServer.waitForAuthentication(2000));

    pumpTransport(processor, buffer, midi, 4);

    const auto roomState = processor.getRoomUiState();
    CHECK(roomState.connected);
    CHECK(roomState.topic.empty());
    CHECK(roomState.visibleFeed.empty());
    CHECK_FALSE(roomState.bpmVote.pending);
    CHECK_FALSE(roomState.bpiVote.pending);

    processor.handleConnectionEvent(ConnectionEvent {
        .type = ConnectionEventType::Connected,
        .reason = "fresh session authenticated",
    });
}
