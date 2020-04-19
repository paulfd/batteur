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
    auto beat = BeatDescription::buildFromFile(fs::current_path() / "tests/files/088 Ballad.json", ec);
    REQUIRE( beat );
}

TEST_CASE("[Files] Existing file 2 ")
{
    std::error_code ec;
    auto beat = BeatDescription::buildFromFile(fs::current_path() / "tests/files/080 Slow Blues 12-8.json", ec);
    REQUIRE( beat );
}