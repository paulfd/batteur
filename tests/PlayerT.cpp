#include "Player.h"
#include "catch.hpp"
#include <algorithm>
#include <string>

using namespace Catch::literals;
using namespace batteur;

namespace {

constexpr double sampleRate { 48000.0 };
constexpr double testBpm { 120.0 };
// At 120 bpm and 48 kHz a quarter note lasts exactly half a second
constexpr int samplesPerQuarter { 24000 };
constexpr int samplesPerBar { 4 * samplesPerQuarter };

/// Stops half a beat short of `samples`. When a recording window starts right
/// on the downbeat, the note sitting exactly on its far edge may be dispatched
/// either side of it depending on how the position rounds; shortening the
/// window keeps the count unambiguous.
constexpr int almost(int samples) { return samples - samplesPerQuarter / 2; }

// Note numbers, one per sequence, so that the origin of every event is
// unambiguous in the recorded timeline.
constexpr uint8_t introNote { 40 };
constexpr uint8_t firstPartNote { 36 };
constexpr uint8_t secondPartNote { 37 };
constexpr uint8_t firstFillNote { 38 };
constexpr uint8_t secondFillNote { 39 };
constexpr uint8_t transitionNote { 45 };
constexpr uint8_t endingNote { 49 };

Sequence makeSequence(uint8_t number, std::initializer_list<double> timestamps, float velocity = 0.8f)
{
    Sequence sequence;
    for (double timestamp : timestamps)
        sequence.emplace_back(timestamp, 0.25, number, velocity);
    return sequence;
}

/// A single bar of quarter notes, looping forever
Sequence makeLoop(uint8_t number)
{
    return makeSequence(number, { 0.0, 1.0, 2.0, 3.0 });
}

/// Eight sixteenth notes filling the second half of a bar
Sequence makeFill(uint8_t number)
{
    return makeSequence(number, { 2.0, 2.25, 2.5, 2.75, 3.0, 3.25, 3.5, 3.75 });
}

/// Appends a part looping on a single note, so that the part being heard is
/// unambiguous
void addPart(BeatDescription& beat, const char* name, uint8_t number)
{
    Part part;
    part.name = name;
    part.mainLoop = makeLoop(number);
    beat.parts.push_back(std::move(part));
}

BeatDescription makeBeat()
{
    BeatDescription beat;
    beat.name = "Test beat";
    beat.group = "Tests";
    beat.bpm = static_cast<float>(testBpm);
    beat.quartersPerBar = 4.0;
    beat.signature = { 4, 4 };
    addPart(beat, "First", firstPartNote);
    return beat;
}

/// A two part beat, the second one looping on `secondPartNote`
BeatDescription makeTwoPartBeat()
{
    auto beat = makeBeat();
    addPart(beat, "Second", secondPartNote);
    return beat;
}

/// Records every note event the player emits on an absolute sample timeline.
class Recorder {
public:
    struct Event {
        long long time;
        uint8_t number;
        float velocity;
    };

    explicit Recorder(Player& player)
    {
        player.setNoteCallback([this](int delay, uint8_t number, float velocity) {
            events.push_back({ elapsed + delay, number, velocity });
        });
    }

    void tick(Player& player, int sampleCount)
    {
        player.tick(sampleCount);
        elapsed += sampleCount;
    }

    /// Ticks for a total of `sampleCount` samples, in blocks of `blockSize`
    void run(Player& player, int sampleCount, int blockSize = 256)
    {
        for (int done = 0; done < sampleCount; done += blockSize)
            tick(player, std::min(blockSize, sampleCount - done));
    }

    std::vector<Event> notesOn() const
    {
        std::vector<Event> returned;
        for (const Event& event : events)
            if (event.velocity > 0.0f)
                returned.push_back(event);
        return returned;
    }

    std::vector<long long> onsetsOf(uint8_t number) const
    {
        std::vector<long long> returned;
        for (const Event& event : notesOn())
            if (event.number == number)
                returned.push_back(event.time);
        return returned;
    }

    size_t countOf(uint8_t number) const { return onsetsOf(number).size(); }

    bool heard(uint8_t number) const { return countOf(number) > 0; }

    void clear() { events.clear(); }

    std::vector<Event> events;
    long long elapsed { 0 };
};

/// A player wired up to a recorder, a fixed sample rate and a loaded beat.
struct Fixture {
    explicit Fixture(const BeatDescription& beat)
        : recorder(player)
    {
        player.setSampleRate(sampleRate);
        REQUIRE( player.loadBeatDescription(beat) );
    }

    /// Starts the player and returns once the transport is actually running
    void startAndSettle()
    {
        player.start();
        recorder.tick(player, 64);
    }

    Player player;
    Recorder recorder;
};

/// Ticks in small blocks until `predicate` holds, rather than guessing how
/// many samples a given transition takes.
template <class F>
void runUntil(Fixture& fixture, F predicate)
{
    for (int i = 0; i < 20000; ++i) {
        if (predicate())
            return;
        fixture.recorder.tick(fixture.player, 64);
    }
    FAIL("The player never reached the expected state");
}

}

TEST_CASE("[Player] Fresh player")
{
    Player player;
    REQUIRE( player.getState() == Player::State::Stopped );
    REQUIRE( !player.isPlaying() );
    REQUIRE( player.getBeatDescription() == nullptr );
    REQUIRE( player.getCurrentSequence() == nullptr );
    REQUIRE( player.getCurrentPartName() == nullptr );
    REQUIRE( player.getTempo() == 120.0_a );
    REQUIRE( player.getBarPosition() == 0.0_a );
    REQUIRE( player.getSequencePosition() == 0.0_a );
    REQUIRE( player.getPartIndex() == 0 );
    REQUIRE( player.getFillIndex() == 0 );
}

TEST_CASE("[Player] Ticking without a beat does nothing")
{
    Player player;
    Recorder recorder { player };
    player.start();
    recorder.run(player, samplesPerBar);
    REQUIRE( recorder.events.empty() );
    REQUIRE( player.getState() == Player::State::Stopped );
}

TEST_CASE("[Player] A beat without parts is rejected")
{
    BeatDescription beat;
    beat.name = "Empty";
    beat.bpm = 100.0f;
    beat.quartersPerBar = 4.0;

    Player player;
    REQUIRE( !player.loadBeatDescription(beat) );
    REQUIRE( player.getBeatDescription() == nullptr );
}

TEST_CASE("[Player] Loading a beat picks up its tempo")
{
    auto beat = makeBeat();
    beat.bpm = 90.0f;

    Player player;
    REQUIRE( player.loadBeatDescription(beat) );
    REQUIRE( player.getBeatDescription() == &beat );
    REQUIRE( player.getTempo() == 90.0_a );
    REQUIRE( player.getState() == Player::State::Stopped );
    REQUIRE( std::string(player.getCurrentPartName()) == "First" );
}

TEST_CASE("[Player] Transport commands only take effect on the next tick")
{
    auto beat = makeBeat();
    Fixture fixture { beat };

    REQUIRE( fixture.player.start() );
    REQUIRE( fixture.player.getState() == Player::State::Stopped );
    fixture.recorder.tick(fixture.player, 64);
    REQUIRE( fixture.player.getState() == Player::State::Playing );
    REQUIRE( fixture.player.isPlaying() );
    REQUIRE( fixture.player.getCurrentSequence() == &beat.parts[0].mainLoop );
}

TEST_CASE("[Player] Starting twice is a no-op")
{
    auto beat = makeBeat();
    Fixture fixture { beat };
    fixture.player.start();

    fixture.recorder.run(fixture.player, samplesPerBar / 2);
    fixture.player.start();
    fixture.recorder.run(fixture.player, almost(samplesPerBar / 2));

    REQUIRE( fixture.player.getState() == Player::State::Playing );
    // The extra start did not requeue the main loop
    REQUIRE( fixture.recorder.countOf(firstPartNote) == 4 );
}

TEST_CASE("[Player] A single bar is played back in time")
{
    auto beat = makeBeat();
    Fixture fixture { beat };
    // The start message is handled at the top of the tick, so the downbeat
    // lands at the very beginning of the first block
    fixture.player.start();
    fixture.recorder.tick(fixture.player, samplesPerBar);

    const auto onsets = fixture.recorder.onsetsOf(firstPartNote);
    REQUIRE( onsets.size() == 4 );
    REQUIRE( onsets[0] == 0 );
    REQUIRE( onsets[1] == samplesPerQuarter );
    REQUIRE( onsets[2] == 2 * samplesPerQuarter );
    REQUIRE( onsets[3] == 3 * samplesPerQuarter );

    // Each 0.25-quarter note has a corresponding note-off at the right time.
    REQUIRE( fixture.recorder.events.size() == 8 );
    std::vector<long long> offTimes;
    for (const auto& event : fixture.recorder.events) {
        REQUIRE( event.number == firstPartNote );
        if (event.velocity == 0.0f)
            offTimes.push_back(event.time);
    }
    REQUIRE( offTimes.size() == onsets.size() );
    for (size_t i = 0; i < onsets.size(); ++i)
        REQUIRE( std::abs(offTimes[i] - onsets[i] - samplesPerQuarter / 4) <= 1 );
}

TEST_CASE("[Player] The main loop keeps looping")
{
    auto beat = makeBeat();
    Fixture fixture { beat };
    fixture.startAndSettle();
    fixture.recorder.clear();

    fixture.recorder.run(fixture.player, 8 * samplesPerBar);
    REQUIRE( fixture.recorder.countOf(firstPartNote) == 32 );
    REQUIRE( fixture.player.getState() == Player::State::Playing );
}

TEST_CASE("[Player] Playback is independent of the block size")
{
    auto beat = makeBeat();

    const auto onsetsWithBlockSize = [&beat](int blockSize) {
        Fixture fixture { beat };
        fixture.player.start();
        fixture.recorder.run(fixture.player, almost(4 * samplesPerBar), blockSize);
        return fixture.recorder.onsetsOf(firstPartNote);
    };

    std::vector<long long> expected;
    for (int quarter = 0; quarter < 16; ++quarter)
        expected.push_back(quarter * samplesPerQuarter);

    // The player rounds quarter positions to integral samples; different
    // block boundaries can move an onset by one sample, but not a beat.
    for (int blockSize : { 32, 256, 1024, samplesPerBar }) {
        const auto onsets = onsetsWithBlockSize(blockSize);
        REQUIRE( onsets.size() == expected.size() );
        for (size_t i = 0; i < expected.size(); ++i)
            REQUIRE( std::abs(onsets[i] - expected[i]) <= 1 );
    }
}

TEST_CASE("[Player] Quarters are stretched over twice as many samples")
{
    // Halving the tempo and doubling the sample rate both double the number
    // of samples a quarter note covers.
    auto beat = makeBeat();
    Fixture fixture { beat };

    SECTION("By halving the tempo")
    {
        fixture.player.setTempo(60.0);
        REQUIRE( fixture.player.getTempo() == 60.0_a );
    }

    SECTION("By doubling the sample rate")
    {
        fixture.player.setSampleRate(2.0 * sampleRate);
    }

    fixture.player.start();
    fixture.recorder.tick(fixture.player, 2 * samplesPerBar);

    const auto onsets = fixture.recorder.onsetsOf(firstPartNote);
    REQUIRE( onsets.size() == 4 );
    REQUIRE( onsets[0] == 0 );
    REQUIRE( onsets[1] == 2 * samplesPerQuarter );
    REQUIRE( onsets[3] == 6 * samplesPerQuarter );
}

TEST_CASE("[Player] Velocities are clamped to the unit range")
{
    auto beat = makeBeat();
    beat.parts[0].mainLoop = makeSequence(firstPartNote, { 0.0, 1.0 }, 2.0f);
    beat.parts[0].mainLoop.emplace_back(2.0, 0.25, secondPartNote, -1.0f);

    Fixture fixture { beat };
    fixture.startAndSettle();
    fixture.recorder.clear();
    fixture.recorder.tick(fixture.player, samplesPerBar);

    for (const auto& event : fixture.recorder.events)
        REQUIRE( (event.velocity >= 0.0f && event.velocity <= 1.0f) );

    REQUIRE( fixture.recorder.countOf(firstPartNote) == 2 );
    for (const auto& event : fixture.recorder.notesOn())
        REQUIRE( event.velocity == 1.0_a );
}

TEST_CASE("[Player] Notes close enough to each other are merged")
{
    auto beat = makeBeat();
    // The second note is 1/100th of a quarter away from the first one, well
    // below the 1/20th merging threshold, while the third one is far away.
    beat.parts[0].mainLoop = makeSequence(firstPartNote, { 0.0, 0.01, 2.0 });

    Fixture fixture { beat };
    fixture.player.start();
    fixture.recorder.tick(fixture.player, samplesPerBar);

    const auto onsets = fixture.recorder.onsetsOf(firstPartNote);
    REQUIRE( onsets.size() == 2 );
    REQUIRE( onsets[0] == 0 );
    REQUIRE( onsets[1] == 2 * samplesPerQuarter );
}

TEST_CASE("[Player] Intros are played once")
{
    auto beat = makeBeat();
    beat.intro = makeSequence(introNote, { 0.0, 1.0, 2.0 });

    Fixture fixture { beat };
    fixture.startAndSettle();
    REQUIRE( fixture.player.getState() == Player::State::Intro );
    REQUIRE( fixture.player.getCurrentSequence() == &*beat.intro );

    fixture.recorder.run(fixture.player, 4 * samplesPerBar);
    REQUIRE( fixture.player.getState() == Player::State::Playing );
    REQUIRE( fixture.player.getCurrentSequence() == &beat.parts[0].mainLoop );
    REQUIRE( fixture.recorder.countOf(introNote) == 3 );
    REQUIRE( fixture.recorder.heard(firstPartNote) );
}

TEST_CASE("[Player] Stopping resets the transport")
{
    auto beat = makeBeat();
    const bool withEnding = GENERATE(true, false);
    CAPTURE( withEnding );
    if (withEnding)
        beat.ending = makeSequence(endingNote, { 0.0, 1.0, 2.0, 3.0 });

    Fixture fixture { beat };
    fixture.startAndSettle();
    fixture.recorder.run(fixture.player, samplesPerBar / 2);

    REQUIRE( fixture.player.stop() );
    fixture.recorder.tick(fixture.player, 64);
    REQUIRE( fixture.player.getState() == Player::State::Ending );

    fixture.recorder.run(fixture.player, 4 * samplesPerBar);
    REQUIRE( fixture.player.getState() == Player::State::Stopped );
    REQUIRE( !fixture.player.isPlaying() );
    REQUIRE( fixture.player.getPartIndex() == 0 );
    REQUIRE( fixture.player.getFillIndex() == 0 );
    REQUIRE( fixture.recorder.heard(endingNote) == withEnding );

    // Nothing plays any more once stopped
    const auto eventCount = fixture.recorder.events.size();
    fixture.recorder.run(fixture.player, 4 * samplesPerBar);
    REQUIRE( fixture.recorder.events.size() == eventCount );
}

TEST_CASE("[Player] Stopping a stopped player is a no-op")
{
    auto beat = makeBeat();
    Fixture fixture { beat };
    fixture.player.stop();
    fixture.recorder.run(fixture.player, samplesPerBar);
    REQUIRE( fixture.player.getState() == Player::State::Stopped );
    REQUIRE( fixture.recorder.events.empty() );
}

TEST_CASE("[Player] Fill-ins go back to the main loop")
{
    auto beat = makeBeat();
    beat.parts[0].fills.push_back(makeFill(firstFillNote));

    Fixture fixture { beat };
    fixture.startAndSettle();
    REQUIRE( fixture.player.getFillIndex() == 0 );

    REQUIRE( fixture.player.fillIn() );
    fixture.recorder.tick(fixture.player, 64);
    REQUIRE( fixture.player.getState() == Player::State::Fill );
    // The fill index already points at the fill to use next time
    REQUIRE( fixture.player.getFillIndex() == 0 );

    fixture.recorder.run(fixture.player, 4 * samplesPerBar);
    REQUIRE( fixture.player.getState() == Player::State::Playing );
    REQUIRE( fixture.recorder.heard(firstFillNote) );
    REQUIRE( fixture.recorder.heard(firstPartNote) );
    REQUIRE( fixture.player.getCurrentSequence() == &beat.parts[0].mainLoop );
}

TEST_CASE("[Player] Fills are cycled through in order")
{
    auto beat = makeBeat();
    beat.parts[0].fills.push_back(makeFill(firstFillNote));
    beat.parts[0].fills.push_back(makeFill(secondFillNote));

    Fixture fixture { beat };
    fixture.startAndSettle();

    fixture.player.fillIn();
    fixture.recorder.tick(fixture.player, 64);
    REQUIRE( fixture.player.getFillIndex() == 1 );
    fixture.recorder.run(fixture.player, 4 * samplesPerBar);
    REQUIRE( fixture.recorder.heard(firstFillNote) );
    REQUIRE( !fixture.recorder.heard(secondFillNote) );

    fixture.player.fillIn();
    fixture.recorder.tick(fixture.player, 64);
    // Wraps back around to the first fill
    REQUIRE( fixture.player.getFillIndex() == 0 );
    fixture.recorder.run(fixture.player, 4 * samplesPerBar);
    REQUIRE( fixture.recorder.heard(secondFillNote) );
}

TEST_CASE("[Player] Filling in on a part without fills does nothing")
{
    auto beat = makeBeat();
    Fixture fixture { beat };
    fixture.startAndSettle();

    fixture.player.fillIn();
    fixture.recorder.run(fixture.player, 2 * samplesPerBar);
    REQUIRE( fixture.player.getState() == Player::State::Playing );
    REQUIRE( fixture.player.getFillIndex() == 0 );
}

TEST_CASE("[Player] Filling in is ignored unless playing")
{
    auto beat = makeBeat();
    beat.intro = makeSequence(introNote, { 0.0, 1.0, 2.0 });
    beat.parts[0].fills.push_back(makeFill(firstFillNote));

    Fixture fixture { beat };

    SECTION("While stopped")
    {
        fixture.player.fillIn();
        fixture.recorder.run(fixture.player, samplesPerBar);
        REQUIRE( fixture.player.getState() == Player::State::Stopped );
        REQUIRE( fixture.recorder.events.empty() );
    }

    SECTION("During the intro")
    {
        fixture.startAndSettle();
        REQUIRE( fixture.player.getState() == Player::State::Intro );
        fixture.player.fillIn();
        fixture.recorder.tick(fixture.player, 64);
        REQUIRE( fixture.player.getState() == Player::State::Intro );
        REQUIRE( fixture.player.getFillIndex() == 0 );
    }
}

TEST_CASE("[Player] Moving to the next part")
{
    auto beat = makeTwoPartBeat();

    Fixture fixture { beat };
    fixture.startAndSettle();
    REQUIRE( fixture.player.getPartIndex() == 0 );

    REQUIRE( fixture.player.next() );
    fixture.recorder.tick(fixture.player, 64);
    REQUIRE( fixture.player.getState() == Player::State::Next );
    REQUIRE( fixture.player.getPartIndex() == 1 );
    REQUIRE( std::string(fixture.player.getCurrentPartName()) == "Second" );

    fixture.recorder.run(fixture.player, 4 * samplesPerBar);
    REQUIRE( fixture.player.getState() == Player::State::Playing );
    REQUIRE( fixture.player.getCurrentSequence() == &beat.parts[1].mainLoop );
    REQUIRE( fixture.recorder.heard(secondPartNote) );

    // The first part is not heard from again
    fixture.recorder.clear();
    fixture.recorder.run(fixture.player, 4 * samplesPerBar);
    REQUIRE( !fixture.recorder.heard(firstPartNote) );
    REQUIRE( fixture.recorder.countOf(secondPartNote) == 16 );
}

TEST_CASE("[Player] The part index wraps around")
{
    auto beat = makeTwoPartBeat();

    Fixture fixture { beat };
    fixture.startAndSettle();

    fixture.player.next();
    fixture.recorder.run(fixture.player, 4 * samplesPerBar);
    REQUIRE( fixture.player.getPartIndex() == 1 );

    fixture.player.next();
    fixture.recorder.run(fixture.player, 4 * samplesPerBar);
    REQUIRE( fixture.player.getPartIndex() == 0 );
    REQUIRE( fixture.player.getState() == Player::State::Playing );
}

TEST_CASE("[Player] Transitions are played when changing parts")
{
    auto beat = makeTwoPartBeat();
    beat.parts[0].transition = makeFill(transitionNote);

    Fixture fixture { beat };
    fixture.startAndSettle();

    fixture.player.next();
    fixture.recorder.run(fixture.player, 4 * samplesPerBar);
    REQUIRE( fixture.recorder.heard(transitionNote) );
    REQUIRE( fixture.recorder.heard(secondPartNote) );
    REQUIRE( fixture.player.getState() == Player::State::Playing );
}

TEST_CASE("[Player] Moving to the next part is ignored unless playing")
{
    auto beat = makeTwoPartBeat();

    Fixture fixture { beat };
    fixture.player.next();
    fixture.recorder.run(fixture.player, samplesPerBar);
    REQUIRE( fixture.player.getState() == Player::State::Stopped );
    REQUIRE( fixture.player.getPartIndex() == 0 );
}

TEST_CASE("[Player] A single part loops back onto itself")
{
    auto beat = makeBeat();
    Fixture fixture { beat };
    fixture.startAndSettle();

    fixture.player.next();
    fixture.recorder.run(fixture.player, 4 * samplesPerBar);
    REQUIRE( fixture.player.getPartIndex() == 0 );
    REQUIRE( fixture.player.getState() == Player::State::Playing );
    REQUIRE( fixture.recorder.heard(firstPartNote) );
}

TEST_CASE("[Player] allOff resets the transport")
{
    auto beat = makeBeat();
    beat.parts[0].fills.push_back(makeFill(firstFillNote));

    Fixture fixture { beat };
    fixture.startAndSettle();
    fixture.player.fillIn();
    fixture.recorder.run(fixture.player, samplesPerBar);

    fixture.player.allOff();
    REQUIRE( fixture.player.getState() == Player::State::Stopped );
    REQUIRE( !fixture.player.isPlaying() );
    REQUIRE( fixture.player.getPartIndex() == 0 );
    REQUIRE( fixture.player.getFillIndex() == 0 );
    REQUIRE( fixture.player.getSequencePosition() == 0.0_a );
    REQUIRE( fixture.player.getCurrentSequence() == nullptr );

    fixture.recorder.clear();
    fixture.recorder.run(fixture.player, 2 * samplesPerBar);
    REQUIRE( fixture.recorder.events.empty() );

    // And the player can be restarted afterwards
    fixture.startAndSettle();
    REQUIRE( fixture.player.getState() == Player::State::Playing );
}

TEST_CASE("[Player] Loading a new beat resets the transport")
{
    auto first = makeBeat();
    auto second = makeBeat();
    second.parts[0].name = "Other";
    second.parts[0].mainLoop = makeLoop(secondPartNote);
    second.bpm = 140.0f;

    Fixture fixture { first };
    fixture.startAndSettle();
    fixture.recorder.run(fixture.player, samplesPerBar);

    REQUIRE( fixture.player.loadBeatDescription(second) );
    REQUIRE( fixture.player.getState() == Player::State::Stopped );
    REQUIRE( fixture.player.getTempo() == 140.0_a );
    REQUIRE( fixture.player.getBeatDescription() == &second );
    REQUIRE( std::string(fixture.player.getCurrentPartName()) == "Other" );

    fixture.startAndSettle();
    fixture.recorder.clear();
    fixture.recorder.run(fixture.player, 2 * samplesPerBar);
    REQUIRE( fixture.recorder.heard(secondPartNote) );
    REQUIRE( !fixture.recorder.heard(firstPartNote) );
}

TEST_CASE("[Player] The sequence position tracks the bar")
{
    auto beat = makeBeat();
    Fixture fixture { beat };
    fixture.startAndSettle();

    fixture.recorder.run(fixture.player, samplesPerQuarter * 2);
    const auto position = fixture.player.getSequencePosition();
    REQUIRE( position > 1.5 );
    REQUIRE( position < 2.5 );
    REQUIRE( fixture.player.getBarPosition() == Approx(position).margin(1e-9) );

    // The bar position stays inside the bar as the loop wraps around
    for (int bar = 0; bar < 4; ++bar) {
        fixture.recorder.run(fixture.player, samplesPerBar);
        REQUIRE( fixture.player.getBarPosition() >= 0.0 );
        REQUIRE( fixture.player.getBarPosition() < 4.0 );
    }
}

TEST_CASE("[Player] Suspending the callback freezes the transport")
{
    auto beat = makeBeat();
    Fixture fixture { beat };
    fixture.startAndSettle();
    const auto position = fixture.player.getSequencePosition();

    fixture.player.suspendCallback();
    fixture.recorder.clear();
    fixture.recorder.run(fixture.player, 2 * samplesPerBar);
    REQUIRE( fixture.recorder.events.empty() );
    REQUIRE( fixture.player.getSequencePosition() == Approx(position) );
    fixture.player.resumeCallback();

    fixture.recorder.run(fixture.player, samplesPerBar);
    REQUIRE( fixture.recorder.countOf(firstPartNote) == 4 );
}

TEST_CASE("[Player] The message queue is bounded")
{
    auto beat = makeBeat();
    beat.parts[0].fills.push_back(makeFill(firstFillNote));
    Fixture fixture { beat };
    fixture.startAndSettle();

    int accepted = 0;
    for (int i = 0; i < 128; ++i)
        if (fixture.player.fillIn())
            accepted++;

    // Excess messages are dropped rather than blocking the caller
    REQUIRE( accepted > 0 );
    REQUIRE( accepted < 128 );

    // And the player still behaves afterwards
    fixture.recorder.run(fixture.player, 4 * samplesPerBar);
    REQUIRE( fixture.player.isPlaying() );
}

TEST_CASE("[Player] Multi-bar sequences")
{
    auto beat = makeBeat();
    beat.parts[0].mainLoop = makeSequence(firstPartNote,
        { 0.0, 1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0 });

    Fixture fixture { beat };
    fixture.player.start();

    // Two bars worth of samples covers exactly one pass of the loop
    fixture.recorder.tick(fixture.player, 2 * samplesPerBar);
    const auto onsets = fixture.recorder.onsetsOf(firstPartNote);
    REQUIRE( onsets.size() == 8 );
    REQUIRE( onsets[7] == 7 * samplesPerQuarter );

    fixture.recorder.run(fixture.player, almost(6 * samplesPerBar));
    REQUIRE( fixture.recorder.countOf(firstPartNote) == 32 );
}

TEST_CASE("[Player] Odd time signatures")
{
    auto beat = makeBeat();
    beat.quartersPerBar = 3.5;
    beat.signature = { 7, 8 };
    beat.parts[0].mainLoop = makeSequence(firstPartNote,
        { 0.0, 0.5, 1.0, 1.5, 2.0, 2.5, 3.0 });

    Fixture fixture { beat };
    fixture.startAndSettle();
    fixture.recorder.clear();

    // Four bars of 3.5 quarters
    fixture.recorder.run(fixture.player, 14 * samplesPerQuarter);
    REQUIRE( fixture.recorder.countOf(firstPartNote) == 28 );
    REQUIRE( fixture.player.getState() == Player::State::Playing );
}

TEST_CASE("[Player] Stopping while a fill is queued")
{
    auto beat = makeBeat();
    beat.parts[0].fills.push_back(makeFill(firstFillNote));
    beat.ending = makeSequence(endingNote, { 0.0, 1.0, 2.0, 3.0 });

    Fixture fixture { beat };
    fixture.startAndSettle();
    fixture.player.fillIn();
    fixture.recorder.tick(fixture.player, 64);
    REQUIRE( fixture.player.getState() == Player::State::Fill );

    // The queued fill is dropped in favour of the ending
    fixture.player.stop();
    fixture.recorder.tick(fixture.player, 64);
    REQUIRE( fixture.player.getState() == Player::State::Ending );

    fixture.recorder.run(fixture.player, 4 * samplesPerBar);
    REQUIRE( fixture.player.getState() == Player::State::Stopped );
    REQUIRE( fixture.recorder.heard(endingNote) );
}

TEST_CASE("[Player] A queued fill is replaced by the transition")
{
    auto beat = makeTwoPartBeat();
    beat.parts[0].fills.push_back(makeFill(firstFillNote));
    beat.parts[0].transition = makeFill(transitionNote);

    Fixture fixture { beat };
    fixture.startAndSettle();
    fixture.player.fillIn();
    fixture.recorder.tick(fixture.player, 64);
    REQUIRE( fixture.player.getState() == Player::State::Fill );

    fixture.player.next();
    fixture.recorder.tick(fixture.player, 64);
    REQUIRE( fixture.player.getState() == Player::State::Next );
    REQUIRE( fixture.player.getPartIndex() == 1 );
    // Queueing a part change resets the fill rotation
    REQUIRE( fixture.player.getFillIndex() == 0 );

    fixture.recorder.run(fixture.player, 4 * samplesPerBar);
    REQUIRE( fixture.recorder.heard(transitionNote) );
    REQUIRE( !fixture.recorder.heard(firstFillNote) );
    REQUIRE( fixture.recorder.heard(secondPartNote) );
    REQUIRE( fixture.player.getState() == Player::State::Playing );
}

TEST_CASE("[Player] Changing parts while the fill is already playing")
{
    auto beat = makeTwoPartBeat();
    beat.parts[0].fills.push_back(makeFill(firstFillNote));

    Fixture fixture { beat };
    fixture.startAndSettle();
    fixture.player.fillIn();
    runUntil(fixture, [&] {
        return fixture.player.getCurrentSequence() == &beat.parts[0].fills[0];
    });
    REQUIRE( fixture.player.getState() == Player::State::Fill );

    fixture.player.next();
    fixture.recorder.tick(fixture.player, 64);
    REQUIRE( fixture.player.getState() == Player::State::Next );
    REQUIRE( fixture.player.getPartIndex() == 1 );

    fixture.recorder.run(fixture.player, 4 * samplesPerBar);
    REQUIRE( fixture.player.getState() == Player::State::Playing );
    REQUIRE( fixture.player.getCurrentSequence() == &beat.parts[1].mainLoop );
    REQUIRE( fixture.recorder.heard(secondPartNote) );
}

TEST_CASE("[Player] A fill with a pickup asked for late in the bar starts at once")
{
    auto beat = makeBeat();
    beat.parts[0].mainLoop = makeSequence(firstPartNote,
        { 0.0, 1.0, 2.0, 3.0, 3.6, 3.8 });
    // The fill only starts on the last half beat, just like the point at
    // which it is asked for
    beat.parts[0].fills.push_back(makeSequence(firstFillNote, { 3.5, 3.75 }));

    Fixture fixture { beat };
    fixture.startAndSettle();
    runUntil(fixture, [&] { return fixture.player.getSequencePosition() > 3.55; });

    const auto requestedAt = fixture.recorder.elapsed;
    fixture.player.fillIn();
    fixture.recorder.clear();
    fixture.recorder.tick(fixture.player, 64);

    fixture.recorder.run(fixture.player, 2 * samplesPerBar);
    const auto onsets = fixture.recorder.onsetsOf(firstFillNote);
    REQUIRE( !onsets.empty() );
    // The 3.75 pickup is still ahead of the request; it must play before
    // the current bar ends, rather than waiting for another loop.
    REQUIRE( onsets.front() >= requestedAt );
    REQUIRE( onsets.front() < requestedAt + samplesPerQuarter / 2 );
    REQUIRE( fixture.player.getState() == Player::State::Playing );
}

TEST_CASE("[Player] A fill with a pickup asked for early skips its pickup notes")
{
    auto beat = makeBeat();
    // A two bar fill whose first bar is only a pickup
    beat.parts[0].fills.push_back(makeSequence(firstFillNote,
        { 3.5, 3.75, 4.0, 5.0, 6.0, 7.0 }));

    Fixture fixture { beat };
    fixture.startAndSettle();
    REQUIRE( fixture.player.getSequencePosition() < 1.0 );

    fixture.player.fillIn();
    fixture.recorder.tick(fixture.player, 64);
    // The player advances the fill's position by one bar immediately,
    // skipping notes at 3.5 and 3.75. Its first audible note is at 5.0,
    // one quarter after the downbeat (not a bar of silence).
    fixture.recorder.clear();

    fixture.recorder.run(fixture.player, 3 * samplesPerBar);
    const auto onsets = fixture.recorder.onsetsOf(firstFillNote);
    REQUIRE( !onsets.empty() );
    REQUIRE( onsets.front() == Approx(samplesPerQuarter).margin(2) );
    REQUIRE( fixture.player.getState() == Player::State::Playing );
}
