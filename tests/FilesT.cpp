#include "BeatDescription.h"
#include "catch.hpp"
using namespace Catch::literals;
using namespace batteur;

TEST_CASE("[Files] Empty filename")
{
    std::error_code ec;
    auto beat = BeatDescription::buildFromFile("", ec);
    REQUIRE( !beat );
}

TEST_CASE("[Files] Nonexistent filename")
{
    std::error_code ec;
    auto beat = BeatDescription::buildFromFile("fake_file.json", ec);
    REQUIRE( !beat );
    REQUIRE( ec == BeatDescriptionError::NonexistentFile );
}

TEST_CASE("[Files] Existing file")
{
    std::error_code ec;
    auto beat = BeatDescription::buildFromFile(fs::current_path() / "tests/files/shuffle.json", ec);
    REQUIRE( beat );
    REQUIRE( beat->bpm == 78 );
    REQUIRE( beat->quartersPerBar == 4.0 );
    REQUIRE( beat->intro );
    REQUIRE( beat->parts.size() == 2 );
    REQUIRE( beat->parts[0].name == "Snare" );
    REQUIRE( beat->parts[1].name == "Ride" );
    REQUIRE( beat->parts[0].fills.size() == 2 );
    REQUIRE( beat->parts[1].fills.size() == 1 );
}

TEST_CASE("[Files] Time signature 1")
{
    std::error_code ec;
    auto beat = BeatDescription::buildFromFile(fs::current_path() / "tests/files/sig1.json", ec);
    REQUIRE( beat );
    REQUIRE( beat->quartersPerBar == 4.0 );
    REQUIRE( beat->signature.num == 4 );
    REQUIRE( beat->signature.denom == 4 );
}

TEST_CASE("[Files] Time signature 2")
{
    std::error_code ec;
    auto beat = BeatDescription::buildFromFile(fs::current_path() / "tests/files/sig2.json", ec);
    REQUIRE( beat );
    REQUIRE( beat->quartersPerBar == 6.0 );
    REQUIRE( beat->signature.num == 12 );
    REQUIRE( beat->signature.denom == 8 );
}

TEST_CASE("[Files] Time signature 3")
{
    std::error_code ec;
    auto beat = BeatDescription::buildFromFile(fs::current_path() / "tests/files/sig3.json", ec);
    REQUIRE( beat );
    REQUIRE( beat->quartersPerBar == 3.5 );
    REQUIRE( beat->signature.num == 7 );
    REQUIRE( beat->signature.denom == 8 );
}

TEST_CASE("[Files] Read from string")
{
    std::error_code ec;
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
    auto beat = BeatDescription::buildFromString(fs::current_path() / "tests/files/shuffle.json", file, ec);
    REQUIRE( beat );
    REQUIRE( beat->bpm == 78 );
    REQUIRE( beat->quartersPerBar == 4.0 );
    REQUIRE( beat->intro );
    REQUIRE( beat->parts.size() == 2 );
    REQUIRE( beat->parts[0].name == "Snare" );
    REQUIRE( beat->parts[1].name == "Ride" );
    REQUIRE( beat->parts[0].fills.size() == 2 );
    REQUIRE( beat->parts[1].fills.size() == 1 );
}