#include "batteur.h"
#include "ghc/filesystem.hpp"
#include <catch2/catch_all.hpp>
namespace fs = ghc::filesystem;
using namespace Catch::literals;

TEST_CASE("Empty filename", "[Library]")
{
    batteur_beat_t* beat = batteur_load_beat("");
    REQUIRE( !beat );
}

TEST_CASE("Nonexistent filename", "[Library]")
{
    batteur_beat_t* beat = batteur_load_beat("fake_file.json");
    REQUIRE( !beat );
}

TEST_CASE("Existing file", "[Library]")
{
    const auto path = fs::current_path() / "tests/files/shuffle.json";
    batteur_beat_t* beat = batteur_load_beat(path.c_str());
    REQUIRE( beat );
    REQUIRE( batteur_get_quarters_per_bar(beat) == 4.0 );
    REQUIRE( batteur_get_total_parts(beat) == 2 );
    REQUIRE( batteur_get_beat_tempo(beat) == 78.0f );
    REQUIRE( std::string(batteur_get_part_name(beat, 0)) == "Snare" );
    REQUIRE( std::string(batteur_get_part_name(beat, 1)) == "Ride" );
    REQUIRE( batteur_get_total_fills(beat, 0) == 2 );
    REQUIRE( batteur_get_total_fills(beat, 1) == 1 );
}

TEST_CASE("Time signature 1", "[Library]")
{
    const auto path = fs::current_path() / "tests/files/sig1.json";
    batteur_beat_t* beat = batteur_load_beat(path.c_str());
    REQUIRE( beat );
    REQUIRE( batteur_get_quarters_per_bar(beat) == 4.0 );
    REQUIRE( batteur_get_time_numerator(beat) == 4 );
    REQUIRE( batteur_get_time_denominator(beat) == 4 );
}

TEST_CASE("Time signature 2", "[Library]")
{
    const auto path = fs::current_path() / "tests/files/sig2.json";
    batteur_beat_t* beat = batteur_load_beat(path.c_str());
    REQUIRE( beat );
    REQUIRE( batteur_get_quarters_per_bar(beat) == 6.0 );
    REQUIRE( batteur_get_time_numerator(beat) == 12 );
    REQUIRE( batteur_get_time_denominator(beat) == 8 );
}

TEST_CASE("Time signature 3", "[Library]")
{
        const auto path = fs::current_path() / "tests/files/sig3.json";
    batteur_beat_t* beat = batteur_load_beat(path.c_str());
    REQUIRE( beat );
    REQUIRE( batteur_get_quarters_per_bar(beat) == 3.5 );
    REQUIRE( batteur_get_time_numerator(beat) == 7 );
    REQUIRE( batteur_get_time_denominator(beat) == 8 );
}

TEST_CASE("Read from string", "[Library]")
{
    std::string file = R"(
        {
            "name": "Slow shuffle",
            "group": "Blues",
            "bpm" : 78,
            "quarters_per_bar": 4,
            "intro": { "filename" : "midi/shuffle_intro.mid" },
            "parts": [
                {
                    "name": "Snare",
                    "sequence" : { "filename" : "midi/shuffle_part.mid" },
                    "fills": [
                        { "filename" : "midi/snare_fill.mid" },
                        { "filename" : "midi/snare_fill.mid" }
                    ],
                    "transition": { "filename" : "midi/snare_fill.mid" }
                },
                {
                    "name": "Ride",
                    "sequence" : { "filename" : "midi/shuffle_part_ride.mid" },
                    "fills": [
                        { "filename" : "midi/snare_fill.mid" }
                    ],
                    "transition": { "filename" : "midi/snare_fill.mid" }
                }
            ]
        }
    )";
    auto path = fs::current_path() / "tests/files/shuffle.json";
    auto beat = batteur_load_beat_from_string(path.c_str(), file.c_str());
    REQUIRE( beat );
    REQUIRE( batteur_get_beat_tempo(beat) == 78 );
    REQUIRE( batteur_get_quarters_per_bar(beat) == 4.0 );
    REQUIRE( batteur_get_total_parts(beat) == 2 );
    REQUIRE( std::string(batteur_get_part_name(beat, 0)) == "Snare" );
    REQUIRE( std::string(batteur_get_part_name(beat, 1)) == "Ride" );
    REQUIRE( batteur_get_total_fills(beat, 0) == 2 );
    REQUIRE( batteur_get_total_fills(beat, 1) == 1 );
}

TEST_CASE("Read from string 2" , "[Library]")
{
    std::string file = R"(
        {
        "name": "Shuffle",
        "group": "Blues",
        "bpm": 120.0,
        "quarters_per_bar": 4.0,
        "intro": {
            "notes": [
            { "time": 0.0073, "duration": 0.1250, "number":  42, "velocity": 0.897638 },
            { "time": 1.0052, "duration": 0.1250, "number":  42, "velocity": 0.866142 },
            { "time": 1.9979, "duration": 0.1250, "number":  42, "velocity": 0.866142 },
            { "time": 2.6240, "duration": 0.3750, "number":  38, "velocity": 0.574803 },
            { "time": 3.0083, "duration": 0.6156, "number":  38, "velocity": 0.818898 },
            { "time": 3.6240, "duration": 0.3750, "number":  38, "velocity": 0.811024 },
            { "time": 3.9937, "duration": 0.6250, "number":  49, "velocity": 0.763780 },
            { "time": 3.9948, "duration": 0.6250, "number":  36, "velocity": 0.842520 }
            ]
        },
        "ending": {
            "notes": [
            { "time": 0.0000, "duration": 0.0417, "number":  49, "velocity": 0.740157 },
            { "time": 0.0052, "duration": 0.0417, "number":  36, "velocity": 0.606299 },
            { "time": 0.0104, "duration": 0.0417, "number":  38, "velocity": 0.763780 },
            { "time": 0.6573, "duration": 0.0417, "number":  38, "velocity": 0.795276 },
            { "time": 1.0000, "duration": 0.0417, "number":  49, "velocity": 0.787402 },
            { "time": 1.0073, "duration": 0.0417, "number":  38, "velocity": 0.897638 },
            { "time": 1.6646, "duration": 0.0417, "number":  57, "velocity": 0.755906 },
            { "time": 1.6698, "duration": 0.0417, "number":  36, "velocity": 0.708661 },
            { "time": 1.6750, "duration": 0.0417, "number":  38, "velocity": 0.881890 },
            { "time": 2.6677, "duration": 0.0417, "number":  38, "velocity": 0.803150 },
            { "time": 2.9990, "duration": 0.0417, "number":  38, "velocity": 0.826772 },
            { "time": 3.3198, "duration": 0.0417, "number":  38, "velocity": 0.787402 },
            { "time": 3.6708, "duration": 0.0417, "number":  38, "velocity": 0.929134 },
            { "time": 3.9760, "duration": 0.1250, "number":  55, "velocity": 0.700787 },
            { "time": 4.0021, "duration": 0.1250, "number":  57, "velocity": 0.826772 }
            ]
        },
        "parts": [
            {
            "name": "Hat",
            "sequence": {
                "notes": [
                { "time": 0.0000, "duration": 0.3125, "number":  36, "velocity": 0.755906 },
                { "time": 0.0000, "duration": 0.6208, "number":  42, "velocity": 0.708661 },
                { "time": 0.6615, "duration": 0.3333, "number":  38, "velocity": 0.291339 },
                { "time": 0.6677, "duration": 0.3313, "number":  42, "velocity": 0.527559 },
                { "time": 0.9990, "duration": 0.6250, "number":  42, "velocity": 0.708661 },
                { "time": 1.0094, "duration": 0.6250, "number":  38, "velocity": 0.755906 },
                { "time": 1.6625, "duration": 0.1875, "number":  36, "velocity": 0.716535 },
                { "time": 1.6635, "duration": 0.3333, "number":  42, "velocity": 0.527559 },
                { "time": 1.6740, "duration": 0.3750, "number":  38, "velocity": 0.354331 },
                { "time": 1.9948, "duration": 0.6250, "number":  36, "velocity": 0.779528 },
                { "time": 2.0083, "duration": 0.6125, "number":  42, "velocity": 0.716535 },
                { "time": 2.6656, "duration": 0.3333, "number":  42, "velocity": 0.519685 },
                { "time": 2.6719, "duration": 0.3125, "number":  38, "velocity": 0.291339 },
                { "time": 2.9844, "duration": 0.6250, "number":  38, "velocity": 0.763780 },
                { "time": 3.0021, "duration": 0.6208, "number":  42, "velocity": 0.700787 },
                { "time": 3.6656, "duration": 0.3292, "number":  36, "velocity": 0.755906 },
                { "time": 3.6667, "duration": 0.3750, "number":  38, "velocity": 0.354331 },
                { "time": 3.6750, "duration": 0.3292, "number":  42, "velocity": 0.527559 },
                { "time": 3.9948, "duration": 0.6250, "number":  36, "velocity": 0.755906 },
                { "time": 4.0042, "duration": 0.6240, "number":  42, "velocity": 0.708661 },
                { "time": 4.6615, "duration": 0.3333, "number":  42, "velocity": 0.511811 },
                { "time": 4.6646, "duration": 0.3333, "number":  38, "velocity": 0.283465 },
                { "time": 4.9948, "duration": 0.6208, "number":  42, "velocity": 0.692913 },
                { "time": 5.0000, "duration": 0.6240, "number":  38, "velocity": 0.755906 },
                { "time": 5.6604, "duration": 0.3750, "number":  38, "velocity": 0.362205 },
                { "time": 5.6615, "duration": 0.3333, "number":  42, "velocity": 0.519685 },
                { "time": 5.6677, "duration": 0.3333, "number":  36, "velocity": 0.724409 },
                { "time": 5.9990, "duration": 0.6250, "number":  42, "velocity": 0.708661 },
                { "time": 6.0052, "duration": 0.6250, "number":  36, "velocity": 0.732283 },
                { "time": 6.6573, "duration": 0.3333, "number":  38, "velocity": 0.283465 },
                { "time": 6.6615, "duration": 0.3333, "number":  42, "velocity": 0.527559 },
                { "time": 6.6615, "duration": 0.3750, "number":  36, "velocity": 0.740157 },
                { "time": 6.9990, "duration": 0.6250, "number":  38, "velocity": 0.748031 },
                { "time": 7.0010, "duration": 0.6240, "number":  42, "velocity": 0.700787 },
                { "time": 7.6250, "duration": 0.3750, "number":  38, "velocity": 0.370079 },
                { "time": 7.6250, "duration": 0.3750, "number":  36, "velocity": 0.771654 },
                { "time": 7.6250, "duration": 0.3750, "number":  42, "velocity": 0.519685 }
                ]
            },
            "fills": [
                {
                "notes": [
                    { "time": 2.6615, "duration": 0.0417, "number":  38, "velocity": 0.417323 },
                    { "time": 2.8375, "duration": 0.0417, "number":  38, "velocity": 0.401575 },
                    { "time": 2.9969, "duration": 0.0417, "number":  36, "velocity": 0.574803 },
                    { "time": 3.0010, "duration": 0.0417, "number":  38, "velocity": 0.881890 },
                    { "time": 3.0042, "duration": 0.0417, "number":  44, "velocity": 0.913386 },
                    { "time": 3.6708, "duration": 0.0417, "number":  38, "velocity": 0.881890 },
                    { "time": 4.0000, "duration": 0.1250, "number":  36, "velocity": 0.763780 },
                    { "time": 4.0000, "duration": 0.1250, "number":  49, "velocity": 0.992126 }
                ]
                },
                {
                "notes": [
                    { "time": 2.6833, "duration": 0.0417, "number":  38, "velocity": 0.692913 },
                    { "time": 2.9917, "duration": 0.0417, "number":  36, "velocity": 0.700787 },
                    { "time": 3.0000, "duration": 0.1250, "number":  44, "velocity": 0.889764 },
                    { "time": 3.0031, "duration": 0.0417, "number":  38, "velocity": 0.826772 },
                    { "time": 3.3323, "duration": 0.0417, "number":  38, "velocity": 0.779528 },
                    { "time": 3.6719, "duration": 0.0417, "number":  38, "velocity": 0.850394 },
                    { "time": 3.9917, "duration": 0.1250, "number":  36, "velocity": 0.866142 },
                    { "time": 4.0052, "duration": 0.1250, "number":  49, "velocity": 0.929134 }
                ]
                }
            ],
            "transition": {
                "notes": [
                { "time": 1.0021, "duration": 0.0417, "number":  38, "velocity": 0.795276 },
                { "time": 1.0021, "duration": 0.1250, "number":  36, "velocity": 0.724409 },
                { "time": 1.3219, "duration": 0.0417, "number":  38, "velocity": 0.795276 },
                { "time": 1.6417, "duration": 0.0417, "number":  38, "velocity": 0.866142 },
                { "time": 2.0000, "duration": 0.0417, "number":  45, "velocity": 0.700787 },
                { "time": 2.0031, "duration": 0.0417, "number":  36, "velocity": 0.574803 },
                { "time": 2.3302, "duration": 0.0417, "number":  45, "velocity": 0.622047 },
                { "time": 2.6656, "duration": 0.0417, "number":  45, "velocity": 0.708661 },
                { "time": 2.9937, "duration": 0.0417, "number":  36, "velocity": 0.645669 },
                { "time": 3.0031, "duration": 0.0417, "number":  43, "velocity": 0.716535 },
                { "time": 3.3062, "duration": 0.0417, "number":  43, "velocity": 0.700787 },
                { "time": 3.6448, "duration": 0.0417, "number":  43, "velocity": 0.748031 },
                { "time": 3.9906, "duration": 0.1250, "number":  36, "velocity": 0.834646 },
                { "time": 4.0000, "duration": 0.1250, "number":  56, "velocity": 0.771654 },
                { "time": 4.0031, "duration": 0.1250, "number":  57, "velocity": 0.779528 }
                ]
            }
            },
            {
            "name": "Ride",
            "sequence": {
                "notes": [
                { "time": 0.0000, "duration": 0.3125, "number":  36, "velocity": 0.755906 },
                { "time": 0.0010, "duration": 0.6208, "number":  51, "velocity": 0.708661 },
                { "time": 0.6573, "duration": 0.3333, "number":  38, "velocity": 0.291339 },
                { "time": 0.6687, "duration": 0.3302, "number":  51, "velocity": 0.527559 },
                { "time": 0.9927, "duration": 0.6250, "number":  44, "velocity": 0.874016 },
                { "time": 0.9948, "duration": 0.6250, "number":  38, "velocity": 0.755906 },
                { "time": 0.9990, "duration": 0.6250, "number":  51, "velocity": 0.708661 },
                { "time": 1.6656, "duration": 0.3750, "number":  38, "velocity": 0.354331 },
                { "time": 1.6677, "duration": 0.3302, "number":  51, "velocity": 0.527559 },
                { "time": 1.6688, "duration": 0.1875, "number":  36, "velocity": 0.716535 },
                { "time": 1.9979, "duration": 0.6125, "number":  51, "velocity": 0.716535 },
                { "time": 1.9990, "duration": 0.6250, "number":  36, "velocity": 0.779528 },
                { "time": 2.6573, "duration": 0.3333, "number":  38, "velocity": 0.291339 },
                { "time": 2.6708, "duration": 0.3292, "number":  51, "velocity": 0.519685 },
                { "time": 2.9896, "duration": 0.3750, "number":  44, "velocity": 0.850394 },
                { "time": 3.0000, "duration": 0.6208, "number":  51, "velocity": 0.700787 },
                { "time": 3.0063, "duration": 0.6250, "number":  38, "velocity": 0.763780 },
                { "time": 3.6573, "duration": 0.3750, "number":  38, "velocity": 0.354331 },
                { "time": 3.6646, "duration": 0.3333, "number":  36, "velocity": 0.755906 },
                { "time": 3.6771, "duration": 0.3219, "number":  51, "velocity": 0.527559 },
                { "time": 3.9990, "duration": 0.6240, "number":  51, "velocity": 0.708661 },
                { "time": 4.0094, "duration": 0.6250, "number":  36, "velocity": 0.755906 },
                { "time": 4.6573, "duration": 0.3333, "number":  51, "velocity": 0.511811 },
                { "time": 4.6615, "duration": 0.3333, "number":  38, "velocity": 0.283465 },
                { "time": 4.9948, "duration": 0.6240, "number":  38, "velocity": 0.755906 },
                { "time": 5.0031, "duration": 0.6208, "number":  51, "velocity": 0.692913 },
                { "time": 5.0083, "duration": 0.3750, "number":  44, "velocity": 0.818898 },
                { "time": 5.6625, "duration": 0.3750, "number":  38, "velocity": 0.362205 },
                { "time": 5.6719, "duration": 0.3229, "number":  36, "velocity": 0.724409 },
                { "time": 5.6729, "duration": 0.3177, "number":  51, "velocity": 0.519685 },
                { "time": 5.9906, "duration": 0.6250, "number":  51, "velocity": 0.708661 },
                { "time": 5.9948, "duration": 0.6250, "number":  36, "velocity": 0.732283 },
                { "time": 6.6615, "duration": 0.3333, "number":  38, "velocity": 0.283465 },
                { "time": 6.6708, "duration": 0.3750, "number":  36, "velocity": 0.740157 },
                { "time": 6.6771, "duration": 0.3260, "number":  51, "velocity": 0.527559 },
                { "time": 6.9990, "duration": 0.3750, "number":  44, "velocity": 0.795276 },
                { "time": 7.0031, "duration": 0.6219, "number":  51, "velocity": 0.700787 },
                { "time": 7.0083, "duration": 0.6167, "number":  38, "velocity": 0.748031 },
                { "time": 7.6250, "duration": 0.3750, "number":  38, "velocity": 0.370079 },
                { "time": 7.6250, "duration": 0.3750, "number":  36, "velocity": 0.771654 },
                { "time": 7.6250, "duration": 0.3750, "number":  51, "velocity": 0.519685 }
                ]
            },
            "fills": [
                {
                "notes": [
                    { "time": 2.6615, "duration": 0.0417, "number":  38, "velocity": 0.417323 },
                    { "time": 2.8375, "duration": 0.0417, "number":  38, "velocity": 0.401575 },
                    { "time": 2.9969, "duration": 0.0417, "number":  36, "velocity": 0.574803 },
                    { "time": 3.0010, "duration": 0.0417, "number":  38, "velocity": 0.881890 },
                    { "time": 3.0042, "duration": 0.0417, "number":  44, "velocity": 0.913386 },
                    { "time": 3.6708, "duration": 0.0417, "number":  38, "velocity": 0.881890 },
                    { "time": 4.0000, "duration": 0.1250, "number":  36, "velocity": 0.763780 },
                    { "time": 4.0000, "duration": 0.1250, "number":  49, "velocity": 0.992126 }
                ]
                },
                {
                "notes": [
                    { "time": 2.6833, "duration": 0.0417, "number":  38, "velocity": 0.692913 },
                    { "time": 2.9917, "duration": 0.0417, "number":  36, "velocity": 0.700787 },
                    { "time": 3.0000, "duration": 0.1250, "number":  44, "velocity": 0.889764 },
                    { "time": 3.0031, "duration": 0.0417, "number":  38, "velocity": 0.826772 },
                    { "time": 3.3323, "duration": 0.0417, "number":  38, "velocity": 0.779528 },
                    { "time": 3.6719, "duration": 0.0417, "number":  38, "velocity": 0.850394 },
                    { "time": 3.9917, "duration": 0.1250, "number":  36, "velocity": 0.866142 },
                    { "time": 4.0052, "duration": 0.1250, "number":  49, "velocity": 0.929134 }
                ]
                }
            ],
            "transition": {
                "notes": [
                { "time": 1.0021, "duration": 0.0417, "number":  38, "velocity": 0.795276 },
                { "time": 1.0021, "duration": 0.1250, "number":  36, "velocity": 0.724409 },
                { "time": 1.3219, "duration": 0.0417, "number":  38, "velocity": 0.795276 },
                { "time": 1.6417, "duration": 0.0417, "number":  38, "velocity": 0.866142 },
                { "time": 2.0000, "duration": 0.0417, "number":  45, "velocity": 0.700787 },
                { "time": 2.0031, "duration": 0.0417, "number":  36, "velocity": 0.574803 },
                { "time": 2.3302, "duration": 0.0417, "number":  45, "velocity": 0.622047 },
                { "time": 2.6656, "duration": 0.0417, "number":  45, "velocity": 0.708661 },
                { "time": 2.9937, "duration": 0.0417, "number":  36, "velocity": 0.645669 },
                { "time": 3.0031, "duration": 0.0417, "number":  43, "velocity": 0.716535 },
                { "time": 3.3062, "duration": 0.0417, "number":  43, "velocity": 0.700787 },
                { "time": 3.6448, "duration": 0.0417, "number":  43, "velocity": 0.748031 },
                { "time": 3.9906, "duration": 0.1250, "number":  36, "velocity": 0.834646 },
                { "time": 4.0000, "duration": 0.1250, "number":  56, "velocity": 0.771654 },
                { "time": 4.0031, "duration": 0.1250, "number":  57, "velocity": 0.779528 }
                ]
            }
            }
        ]
        }
    )";

    auto path = fs::current_path() / "tests/files/shuffle.json";
    auto beat = batteur_load_beat_from_string(path.c_str(), file.c_str());
    REQUIRE( beat );
    REQUIRE( batteur_get_beat_tempo(beat) == 120 );
    REQUIRE( batteur_get_quarters_per_bar(beat) == 4.0 );
    REQUIRE( batteur_get_total_parts(beat) == 2 );
    REQUIRE( std::string(batteur_get_part_name(beat, 0)) == "Hat" );
    REQUIRE( std::string(batteur_get_part_name(beat, 1)) == "Ride" );
    REQUIRE( batteur_get_total_fills(beat, 0) == 2 );
    REQUIRE( batteur_get_total_fills(beat, 0) == 2 );

}
