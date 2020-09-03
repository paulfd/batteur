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
    REQUIRE( beat->quartersPerBar == 4 );
    REQUIRE( beat->intro );
    REQUIRE( beat->parts.size() == 2 );
    REQUIRE( beat->parts[0].name == "Snare" );
    REQUIRE( beat->parts[1].name == "Ride" );
    REQUIRE( beat->parts[0].fills.size() == 2 );
    REQUIRE( beat->parts[1].fills.size() == 1 );
}
