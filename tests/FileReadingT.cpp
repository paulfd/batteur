#include "FileReadingHelpers.h"
#include "catch.hpp"
using namespace Catch::literals;
using namespace nlohmann;

TEST_CASE("[Files] BPM Not present")
{
    const json j;
    REQUIRE( !checkBPM(j).has_value() );
    REQUIRE( checkBPM(j).error() == BPMError::NotPresent );
}

TEST_CASE("[Files] BPM Nan 1")
{
    const auto j = "\"wat\""_json;
    REQUIRE( !checkBPM(j).has_value() );
    REQUIRE( checkBPM(j).error() == BPMError::NotANumber );
}

TEST_CASE("[Files] BPM Nan 2")
{
    const auto j = "\"5.4\""_json;
    REQUIRE( !checkBPM(j).has_value() );
    REQUIRE( checkBPM(j).error() == BPMError::NotANumber );
}
TEST_CASE("[Files] BPM Negative")
{
    const auto j = "-5.4"_json;
    REQUIRE( !checkBPM(j).has_value() );
    REQUIRE( checkBPM(j).error() == BPMError::Negative );
}


TEST_CASE("[Files] BPM OK")
{
    const auto j = "5.4"_json;
    REQUIRE( checkBPM(j).value() == 5.4 );
}

TEST_CASE("[Files] QuartersPerBar Not present")
{
    const json j;
    REQUIRE( !checkQuartersPerBar(j).has_value() );
    REQUIRE( checkQuartersPerBar(j).error() == QuartersPerBarError::NotPresent );
}

TEST_CASE("[Files] QuartersPerBar Nan 1")
{
    const auto j = "\"wat\""_json;
    REQUIRE( !checkQuartersPerBar(j).has_value() );
    REQUIRE( checkQuartersPerBar(j).error() == QuartersPerBarError::NotANumber );
}

TEST_CASE("[Files] QuartersPerBar Nan 2")
{
    const auto j = "\"5.4\""_json;
    REQUIRE( !checkQuartersPerBar(j).has_value() );
    REQUIRE( checkQuartersPerBar(j).error() == QuartersPerBarError::NotANumber );
}

TEST_CASE("[Files] QuartersPerBar Nan 4")
{
    const auto j = "-5"_json;
    REQUIRE( !checkQuartersPerBar(j).has_value() );
    REQUIRE( checkQuartersPerBar(j).error() == QuartersPerBarError::Negative );
}


TEST_CASE("[Files] QuartersPerBar 0")
{
    const auto j = "0"_json;
    REQUIRE( !checkQuartersPerBar(j).has_value() );
    REQUIRE( checkQuartersPerBar(j).error() == QuartersPerBarError::Zero );
}

TEST_CASE("[Files] QuartersPerBar OK")
{
    const auto j = "4"_json;
    REQUIRE( checkQuartersPerBar(j).value() == 4 );
}

TEST_CASE("[Files] ReadMidiFile errors")
{
    SECTION("Empty")
    {
        auto j = "{}"_json;
        const auto f = readSequence(j, fs::current_path() / "tests/files/" );
        REQUIRE( !f.has_value() );
        REQUIRE( f.error() == ReadingError::NotPresent );
    }

    SECTION("Badly written filename")
    {
        auto j = "{\"filenam\": \"wat\"}"_json;
        const auto f = readSequence(j, fs::current_path() / "tests/files/" );
        REQUIRE( !f.has_value() );
        REQUIRE( f.error() == ReadingError::NotPresent );
    }

    SECTION("Nonexistent file")
    {
        auto j = "{\"filename\": \"nonexistent_file.midi\"}"_json;
        const auto f = readSequence(j, fs::current_path() / "tests/files/" );
        REQUIRE( !f.has_value() );
        REQUIRE( f.error() == ReadingError::MidiFileError );
    }

    SECTION("Bad ignore_bars")
    {
        auto j = R"({"filename": "test_file_1.mid", "ignore_bars": -4})"_json;
        const auto f = readSequence(j, fs::current_path() / "tests/files/" );
        REQUIRE( !f.has_value() );
        REQUIRE( f.error() == ReadingError::WrongIgnoreBars );
    }

    SECTION("Bad bars")
    {
        auto j = R"({"filename": "test_file_1.mid", "bars": -4})"_json;
        const auto f = readSequence(j, fs::current_path() / "tests/files/" );
        REQUIRE( !f.has_value() );
        REQUIRE( f.error() == ReadingError::WrongBars );
    }

    SECTION("No remaining notes")
    {
        auto j = R"({"filename": "test_file_1.mid", "ignore_bars": 60})"_json;
        const auto f = readSequence(j, fs::current_path() / "tests/files/" );
        REQUIRE( !f.has_value() );
        REQUIRE( f.error() == ReadingError::NoDataRead );
    }

    SECTION("No remaining notes 2")
    {
        auto j = R"({"filename": "test_file_2.mid", "ignore_bars": 1, "bars": 1})"_json;
        const auto f = readSequence(j, fs::current_path() / "tests/files/" );
        REQUIRE( !f.has_value() );
        REQUIRE( f.error() == ReadingError::NoDataRead );
    }
}

TEST_CASE("[Files] ReadMidiFile files")
{
    SECTION("Test file 1")
    {
        auto j = R"({"filename": "test_file_1.mid"})"_json;
        const auto f = readSequence(j, fs::current_path() / "tests/files/" );
        REQUIRE( f.has_value() );
        REQUIRE( f.value().size() == 5 );
        REQUIRE( batteur::barCount(*f, 4) == 2);
    }

    SECTION("Test file 1 - Ignore 1")
    {
        auto j = R"({"filename": "test_file_1.mid", "ignore_bars": 1})"_json;
        const auto f = readSequence(j, fs::current_path() / "tests/files/" );
        REQUIRE( f.has_value() );
        REQUIRE( f.value().size() == 4 );
        REQUIRE( batteur::barCount(*f, 4) == 1);
    }

    SECTION("Test file 2")
    {
        auto j = R"({"filename": "test_file_2.mid"})"_json;
        const auto f = readSequence(j, fs::current_path() / "tests/files/" );
        REQUIRE( f.has_value() );
        REQUIRE( f->size() == 5 );
        REQUIRE( batteur::barCount(*f, 4) == 4);
    }

    SECTION("Test file 3")
    {
        auto j = R"({"filename": "test_file_3.mid"})"_json;
        const auto f = readSequence(j, fs::current_path() / "tests/files/" );
        REQUIRE( f.has_value() );
        REQUIRE( f.value().size() == 16 );
        REQUIRE( batteur::barCount(*f, 4) == 4);
    }
}