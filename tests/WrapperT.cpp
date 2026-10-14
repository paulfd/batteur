#include "batteur.h"
#include "filesystem.hpp"
#include "catch.hpp"
#include <string>
#include <vector>

using namespace Catch::literals;
namespace fs = ghc::filesystem;

namespace {

constexpr double sampleRate { 48000.0 };
constexpr int samplesPerQuarter { 24000 };
constexpr int samplesPerBar { 4 * samplesPerQuarter };

const char* testBeat = R"(
{
    "name": "Wrapper test",
    "group": "Tests",
    "bpm": 120.0,
    "quarters_per_bar": 4,
    "intro": { "notes": [
        { "time": 0.0, "duration": 0.25, "number": 40, "velocity": 0.8 },
        { "time": 1.0, "duration": 0.25, "number": 40, "velocity": 0.8 },
        { "time": 2.0, "duration": 0.25, "number": 40, "velocity": 0.8 }
    ] },
    "ending": { "notes": [
        { "time": 0.0, "duration": 0.25, "number": 49, "velocity": 0.8 },
        { "time": 1.0, "duration": 0.25, "number": 49, "velocity": 0.8 },
        { "time": 2.0, "duration": 0.25, "number": 49, "velocity": 0.8 },
        { "time": 3.0, "duration": 0.25, "number": 49, "velocity": 0.8 }
    ] },
    "parts": [
        {
            "name": "First",
            "sequence": { "notes": [
                { "time": 0.0, "duration": 0.25, "number": 36, "velocity": 0.8 },
                { "time": 1.0, "duration": 0.25, "number": 36, "velocity": 0.8 },
                { "time": 2.0, "duration": 0.25, "number": 36, "velocity": 0.8 },
                { "time": 3.0, "duration": 0.25, "number": 36, "velocity": 0.8 }
            ] },
            "fills": [
                { "notes": [
                    { "time": 2.0, "duration": 0.25, "number": 38, "velocity": 0.8 },
                    { "time": 2.5, "duration": 0.25, "number": 38, "velocity": 0.8 },
                    { "time": 3.0, "duration": 0.25, "number": 38, "velocity": 0.8 },
                    { "time": 3.5, "duration": 0.25, "number": 38, "velocity": 0.8 }
                ] },
                { "notes": [
                    { "time": 2.0, "duration": 0.25, "number": 39, "velocity": 0.8 },
                    { "time": 3.0, "duration": 0.25, "number": 39, "velocity": 0.8 }
                ] }
            ],
            "transition": { "notes": [
                { "time": 2.0, "duration": 0.25, "number": 45, "velocity": 0.8 },
                { "time": 3.0, "duration": 0.25, "number": 45, "velocity": 0.8 }
            ] }
        },
        {
            "name": "Second",
            "sequence": { "notes": [
                { "time": 0.0, "duration": 0.25, "number": 37, "velocity": 0.8 },
                { "time": 1.0, "duration": 0.25, "number": 37, "velocity": 0.8 },
                { "time": 2.0, "duration": 0.25, "number": 37, "velocity": 0.8 },
                { "time": 3.0, "duration": 0.25, "number": 37, "velocity": 0.8 }
            ] },
            "fills": [
                { "notes": [
                    { "time": 2.0, "duration": 0.25, "number": 38, "velocity": 0.8 },
                    { "time": 3.0, "duration": 0.25, "number": 38, "velocity": 0.8 }
                ] }
            ]
        }
    ]
}
)";

struct Collector {
    std::vector<uint8_t> notesOn;
    int totalEvents { 0 };

    bool heard(uint8_t number) const
    {
        return std::find(notesOn.begin(), notesOn.end(), number) != notesOn.end();
    }
};

void collect(int delay, uint8_t number, float value, void* cbdata)
{
    REQUIRE( delay >= 0 );
    auto* collector = static_cast<Collector*>(cbdata);
    collector->totalEvents++;
    if (value > 0.0f)
        collector->notesOn.push_back(number);
}

/// Owns a beat and a player for the duration of a test case
struct Fixture {
    Fixture()
        : beat(batteur_load_beat_from_string("virtual.json", testBeat))
        , player(batteur_new())
    {
        REQUIRE( beat != nullptr );
        REQUIRE( player != nullptr );
        batteur_note_cb(player, &collect, &collector);
        batteur_set_sample_rate(player, sampleRate);
        REQUIRE( batteur_load(player, beat) );
    }

    ~Fixture()
    {
        batteur_free(player);
        batteur_free_beat(beat);
    }

    void tick(int sampleCount = samplesPerBar, int blockSize = 256)
    {
        for (int done = 0; done < sampleCount; done += blockSize)
            batteur_tick(player, std::min(blockSize, sampleCount - done));
    }

    void startAndSettle()
    {
        batteur_start(player);
        batteur_tick(player, 64);
    }

    batteur_beat_t* beat;
    batteur_player_t* player;
    Collector collector;
};

}

TEST_CASE("[Wrapper] Null beats are handled gracefully")
{
    REQUIRE( batteur_get_beat_name(nullptr) == nullptr );
    REQUIRE( batteur_get_part_name(nullptr, 0) == nullptr );
    REQUIRE( batteur_get_total_parts(nullptr) == 0 );
    REQUIRE( batteur_get_total_fills(nullptr, 0) == 0 );
    REQUIRE( batteur_get_time_numerator(nullptr) == 0 );
    REQUIRE( batteur_get_time_denominator(nullptr) == 0 );
    batteur_free_beat(nullptr);
}

TEST_CASE("[Wrapper] Null players are handled gracefully")
{
    REQUIRE( !batteur_load(nullptr, nullptr) );
    REQUIRE( !batteur_playing(nullptr) );
    REQUIRE( batteur_get_tempo(nullptr) == 120.0_a );
    REQUIRE( batteur_get_current_beat(nullptr) == nullptr );
    REQUIRE( batteur_get_status(nullptr) == BATTEUR_STOPPED );
    REQUIRE( batteur_get_bar_position(nullptr) == 0.0_a );
    REQUIRE( batteur_get_part_index(nullptr) == 0 );
    REQUIRE( batteur_get_fill_index(nullptr) == 0 );

    // None of the commands should dereference the null player
    batteur_set_sample_rate(nullptr, 44100.0);
    batteur_note_cb(nullptr, &collect, nullptr);
    batteur_set_tempo(nullptr, 100.0);
    batteur_tick(nullptr, 256);
    batteur_fill_in(nullptr);
    batteur_next(nullptr);
    batteur_stop(nullptr);
    batteur_start(nullptr);
    batteur_all_off(nullptr);
    batteur_free(nullptr);
}

TEST_CASE("[Wrapper] Loading a beat from a file")
{
    const auto path = (fs::current_path() / "tests/files/shuffle.json").string();
    auto* beat = batteur_load_beat(path.c_str());
    REQUIRE( beat != nullptr );
    REQUIRE( std::string(batteur_get_beat_name(beat)) == "Slow shuffle" );
    REQUIRE( batteur_get_total_parts(beat) == 2 );
    batteur_free_beat(beat);
}

TEST_CASE("[Wrapper] Beat introspection")
{
    auto* beat = batteur_load_beat_from_string("virtual.json", testBeat);
    REQUIRE( beat != nullptr );

    REQUIRE( std::string(batteur_get_beat_name(beat)) == "Wrapper test" );
    REQUIRE( batteur_get_total_parts(beat) == 2 );
    REQUIRE( std::string(batteur_get_part_name(beat, 0)) == "First" );
    REQUIRE( std::string(batteur_get_part_name(beat, 1)) == "Second" );
    REQUIRE( batteur_get_total_fills(beat, 0) == 2 );
    REQUIRE( batteur_get_total_fills(beat, 1) == 1 );
    REQUIRE( batteur_get_time_numerator(beat) == 4 );
    REQUIRE( batteur_get_time_denominator(beat) == 4 );

    SECTION("A signature that is not the default")
    {
        const char* oddMeter = R"({ "name": "Odd", "signature": [7, 8], "parts": [
            { "name": "Only", "sequence": { "notes": [
                { "time": 0.0, "duration": 0.25, "number": 36, "velocity": 0.5 } ] } } ] })";
        auto* odd = batteur_load_beat_from_string("virtual.json", oddMeter);
        REQUIRE( odd != nullptr );
        REQUIRE( batteur_get_time_numerator(odd) == 7 );
        REQUIRE( batteur_get_time_denominator(odd) == 8 );
        batteur_free_beat(odd);
    }

    SECTION("Out of range part indices")
    {
        REQUIRE( batteur_get_part_name(beat, -1) == nullptr );
        REQUIRE( batteur_get_part_name(beat, 2) == nullptr );
        REQUIRE( batteur_get_total_fills(beat, -1) == 0 );
        REQUIRE( batteur_get_total_fills(beat, 2) == 0 );
    }

    batteur_free_beat(beat);
}

TEST_CASE("[Wrapper] Loading a beat into a player")
{
    Fixture fixture;
    REQUIRE( batteur_get_current_beat(fixture.player) == fixture.beat );
    REQUIRE( batteur_get_tempo(fixture.player) == 120.0_a );
    REQUIRE( !batteur_playing(fixture.player) );
    REQUIRE( batteur_get_status(fixture.player) == BATTEUR_STOPPED );

    REQUIRE( !batteur_load(fixture.player, nullptr) );
    REQUIRE( !batteur_load(nullptr, fixture.beat) );

    // The beat tempo is only a starting point
    batteur_set_tempo(fixture.player, 85.0);
    REQUIRE( batteur_get_tempo(fixture.player) == 85.0_a );
}

TEST_CASE("[Wrapper] Running the transport through every state")
{
    Fixture fixture;

    fixture.startAndSettle();
    REQUIRE( batteur_get_status(fixture.player) == BATTEUR_INTRO );
    REQUIRE( batteur_playing(fixture.player) );

    fixture.tick(2 * samplesPerBar);
    REQUIRE( batteur_get_status(fixture.player) == BATTEUR_PLAYING );
    REQUIRE( fixture.collector.heard(40) );
    REQUIRE( fixture.collector.heard(36) );

    batteur_fill_in(fixture.player);
    batteur_tick(fixture.player, 64);
    REQUIRE( batteur_get_status(fixture.player) == BATTEUR_FILL_IN );
    REQUIRE( batteur_get_fill_index(fixture.player) == 1 );
    fixture.tick(2 * samplesPerBar);
    REQUIRE( batteur_get_status(fixture.player) == BATTEUR_PLAYING );
    REQUIRE( fixture.collector.heard(38) );

    batteur_next(fixture.player);
    batteur_tick(fixture.player, 64);
    REQUIRE( batteur_get_status(fixture.player) == BATTEUR_NEXT );
    REQUIRE( batteur_get_part_index(fixture.player) == 1 );
    fixture.tick(3 * samplesPerBar);
    REQUIRE( batteur_get_status(fixture.player) == BATTEUR_PLAYING );
    REQUIRE( fixture.collector.heard(45) );
    REQUIRE( fixture.collector.heard(37) );

    batteur_stop(fixture.player);
    batteur_tick(fixture.player, 64);
    REQUIRE( batteur_get_status(fixture.player) == BATTEUR_ENDING );
    fixture.tick(3 * samplesPerBar);
    REQUIRE( batteur_get_status(fixture.player) == BATTEUR_STOPPED );
    REQUIRE( !batteur_playing(fixture.player) );
    REQUIRE( fixture.collector.heard(49) );
}

TEST_CASE("[Wrapper] The bar position advances while playing")
{
    Fixture fixture;
    fixture.startAndSettle();
    REQUIRE( batteur_get_bar_position(fixture.player) < 0.1 );

    fixture.tick(2 * samplesPerQuarter);
    const auto position = batteur_get_bar_position(fixture.player);
    REQUIRE( position > 1.5 );
    REQUIRE( position < 2.5 );
}

TEST_CASE("[Wrapper] All off resets the player")
{
    Fixture fixture;
    fixture.startAndSettle();
    fixture.tick(2 * samplesPerBar);
    REQUIRE( batteur_playing(fixture.player) );

    batteur_all_off(fixture.player);
    REQUIRE( !batteur_playing(fixture.player) );
    REQUIRE( batteur_get_status(fixture.player) == BATTEUR_STOPPED );
    REQUIRE( batteur_get_part_index(fixture.player) == 0 );
    REQUIRE( batteur_get_fill_index(fixture.player) == 0 );

    const auto eventCount = fixture.collector.totalEvents;
    fixture.tick(2 * samplesPerBar);
    REQUIRE( fixture.collector.totalEvents == eventCount );
}

TEST_CASE("[Wrapper] Beats that fail to load come back as null")
{
    // The C++ tests cover which inputs are rejected and why; these only
    // check that a failure crosses the wrapper as a null handle.
    const auto text = GENERATE(as<std::string> {},
        R"({ "name": "No parts", "parts": [] })",
        R"({ "name": "No parts" })",
        R"({ "name": null, "parts": [] })",
        "{");

    CAPTURE( text );
    REQUIRE( batteur_load_beat_from_string("virtual.json", text.c_str()) == nullptr );

    REQUIRE( batteur_load_beat("this_file_does_not_exist.json") == nullptr );
}
