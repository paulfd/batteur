#include "FileReadingHelpers.h"
#include "catch.hpp"
#include <algorithm>
#include <string>

using namespace Catch::literals;
using namespace nlohmann;

namespace {

fs::path testFiles()
{
    return fs::current_path() / "tests/files/";
}

tl::expected<batteur::Sequence, ReadingError> read(const char* text)
{
    return readSequence(json::parse(text), testFiles());
}

bool sortedByTimestamp(const batteur::Sequence& sequence)
{
    return std::is_sorted(sequence.begin(), sequence.end(),
        [](const batteur::Note& lhs, const batteur::Note& rhs) {
            return lhs.timestamp < rhs.timestamp;
        });
}

}

TEST_CASE("[Files] BPM")
{
    SECTION("Rejected values")
    {
        std::string input;
        BPMError error;
        std::tie(input, error) = GENERATE(table<std::string, BPMError>({
            { "null", BPMError::NotPresent },
            { "\"wat\"", BPMError::NotANumber },
            { "\"5.4\"", BPMError::NotANumber },
            { "-5.4", BPMError::Negative },
            { "0", BPMError::Negative },
        }));

        CAPTURE( input );
        const auto bpm = checkBPM(json::parse(input));
        REQUIRE( !bpm.has_value() );
        REQUIRE( bpm.error() == error );
    }

    SECTION("Accepted values")
    {
        REQUIRE( checkBPM("5.4"_json).value() == 5.4_a );
        REQUIRE( checkBPM("120"_json).value() == 120.0_a );
    }
}

TEST_CASE("[Files] QuartersPerBar")
{
    SECTION("Rejected values")
    {
        std::string input;
        QuartersPerBarError error;
        std::tie(input, error) = GENERATE(table<std::string, QuartersPerBarError>({
            { "null", QuartersPerBarError::NotPresent },
            { "\"wat\"", QuartersPerBarError::NotANumber },
            { "\"5.4\"", QuartersPerBarError::NotANumber },
            { "-5", QuartersPerBarError::Negative },
            { "0", QuartersPerBarError::Zero },
        }));

        CAPTURE( input );
        const auto qpb = checkQuartersPerBar(json::parse(input));
        REQUIRE( !qpb.has_value() );
        REQUIRE( qpb.error() == error );
    }

    SECTION("Accepted values")
    {
        REQUIRE( checkQuartersPerBar("4"_json).value() == 4.0_a );
        REQUIRE( checkQuartersPerBar("3.5"_json).value() == 3.5_a );
    }
}

TEST_CASE("[Files] ReadSequence errors")
{
    std::string input;
    ReadingError error;
    std::tie(input, error) = GENERATE(table<std::string, ReadingError>({
        { "null", ReadingError::NotPresent },
        { "{}", ReadingError::NotPresent },
        // Neither a "filename" nor a "notes" key
        { R"({"filenam": "wat"})", ReadingError::NotPresent },
        { R"({"filename": "nonexistent_file.midi"})", ReadingError::MidiFileError },
        { R"({"filename": "test_file_1.mid", "ignore_bars": -4})", ReadingError::WrongIgnoreBars },
        { R"({"filename": "test_file_1.mid", "bars": -4})", ReadingError::WrongBars },
        { R"({"filename": "test_file_3.mid", "bars": 0})", ReadingError::ZeroBars },
        // Everything in the file is ignored, or falls outside the kept bars
        { R"({"filename": "test_file_1.mid", "ignore_bars": 60})", ReadingError::NoDataRead },
        { R"({"filename": "test_file_2.mid", "ignore_bars": 1, "bars": 1})", ReadingError::NoDataRead },
    }));

    CAPTURE( input );
    const auto f = read(input.c_str());
    REQUIRE( !f.has_value() );
    REQUIRE( f.error() == error );
}

TEST_CASE("[Files] ReadMidiFile")
{
    std::string input;
    size_t noteCount;
    double bars;
    std::tie(input, noteCount, bars) = GENERATE(table<std::string, size_t, double>({
        { R"({"filename": "test_file_1.mid"})", 5, 2 },
        { R"({"filename": "test_file_1.mid", "ignore_bars": 1})", 4, 1 },
        { R"({"filename": "test_file_2.mid"})", 5, 4 },
        { R"({"filename": "test_file_3.mid"})", 16, 4 },
    }));

    CAPTURE( input );
    const auto f = read(input.c_str());
    REQUIRE( f.has_value() );
    REQUIRE( f->size() == noteCount );
    REQUIRE( batteur::barCount(*f, 4) == bars );
    // Timestamps come out of the sequencer in order
    REQUIRE( sortedByTimestamp(*f) );
}

TEST_CASE("[Files] ReadMidiFile bar selection")
{
    const auto whole = read(R"({"filename": "test_file_3.mid"})");
    REQUIRE( whole.has_value() );

    SECTION("Keeping the first bars only")
    {
        const auto twoBars = read(R"({"filename": "test_file_3.mid", "bars": 2})");
        REQUIRE( twoBars.has_value() );
        REQUIRE( batteur::barCount(*twoBars, 4) == 2 );
        REQUIRE( twoBars->size() < whole->size() );
    }

    SECTION("Ignoring and keeping bars")
    {
        const auto f = read(
            R"({"filename": "test_file_3.mid", "ignore_bars": 1, "bars": 2})");
        REQUIRE( f.has_value() );
        REQUIRE( batteur::barCount(*f, 4) == 2 );

        // Compare against the original notes in bars 2 and 3, not just
        // against zero: omitting the timestamp shift must fail this test.
        size_t index = 0;
        for (const auto& note : *whole) {
            if (note.timestamp < 4.0 || note.timestamp > 12.0)
                continue;
            REQUIRE( index < f->size() );
            REQUIRE( (*f)[index].number == note.number );
            REQUIRE( (*f)[index].timestamp == Approx(note.timestamp - 4.0) );
            ++index;
        }
        REQUIRE( index == f->size() );
    }
}

TEST_CASE("[Files] Note offs written as zero velocity note ons")
{
    const auto f = read(R"({"filename": "velocity_zero.mid"})");
    REQUIRE( f.has_value() );
    REQUIRE( f->size() == 3 );

    REQUIRE( (*f)[0].number == 36 );
    REQUIRE( (*f)[0].timestamp == 0.0_a );
    REQUIRE( (*f)[0].duration == 1.0_a );

    // The middle note uses a real note off and behaves identically
    REQUIRE( (*f)[1].number == 38 );
    REQUIRE( (*f)[1].timestamp == 1.0_a );
    REQUIRE( (*f)[1].duration == 1.0_a );

    REQUIRE( (*f)[2].number == 42 );
    REQUIRE( (*f)[2].timestamp == 2.0_a );
    REQUIRE( (*f)[2].duration == 2.0_a );

    REQUIRE( batteur::barCount(*f, 4) == 1 );
}

TEST_CASE("[Files] ReadSequenceByName")
{
    const auto j = R"({
        "intro": { "filename": "test_file_1.mid" },
        "empty": {}
    })"_json;

    SECTION("Existing key")
    {
        const auto f = readSequenceByName(j, testFiles(), "intro");
        REQUIRE( f.has_value() );
        REQUIRE( f->size() == 5 );
    }

    SECTION("Everything else reads as absent")
    {
        const auto name = GENERATE(as<std::string> {},
            "ending", // missing key
            "", // no name at all
            "empty"); // present, but holds no sequence

        CAPTURE( name );
        const auto f = readSequenceByName(j, testFiles(), name);
        REQUIRE( !f.has_value() );
        REQUIRE( f.error() == ReadingError::NotPresent );
    }

    SECTION("Null JSON")
    {
        const auto f = readSequenceByName(json {}, testFiles(), "intro");
        REQUIRE( !f.has_value() );
        REQUIRE( f.error() == ReadingError::NotPresent );
    }
}

TEST_CASE("[Files] ReadNoteList")
{
    SECTION("A well formed note list")
    {
        const auto f = read(R"({"notes": [
            { "time": 0.0, "duration": 0.25, "number": 36, "velocity": 0.5 },
            { "time": 1.5, "duration": 0.125, "number": 42, "velocity": 1.0 }
        ]})");
        REQUIRE( f.has_value() );
        REQUIRE( f->size() == 2 );
        REQUIRE( (*f)[0].timestamp == 0.0_a );
        REQUIRE( (*f)[0].duration == 0.25_a );
        REQUIRE( (*f)[0].number == 36 );
        REQUIRE( (*f)[0].velocity == 0.5_a );
        REQUIRE( (*f)[1].timestamp == 1.5_a );
        REQUIRE( (*f)[1].number == 42 );
    }

    SECTION("Notes are sorted by timestamp")
    {
        const auto f = read(R"({"notes": [
            { "time": 2.0, "duration": 0.25, "number": 36, "velocity": 0.5 },
            { "time": 0.5, "duration": 0.25, "number": 38, "velocity": 0.5 },
            { "time": 1.0, "duration": 0.25, "number": 42, "velocity": 0.5 }
        ]})");
        REQUIRE( f.has_value() );
        REQUIRE( f->size() == 3 );
        REQUIRE( (*f)[0].number == 38 );
        REQUIRE( (*f)[1].number == 42 );
        REQUIRE( (*f)[2].number == 36 );
    }

    SECTION("Rejected note lists")
    {
        std::string notes;
        ReadingError error;
        std::tie(notes, error) = GENERATE(table<std::string, ReadingError>({
            { R"({ "time": 0.0 })", ReadingError::WrongNoteListFormat }, // not an array
            { "[]", ReadingError::NoDataRead },
            { R"([{ "duration": 0.25, "number": 36, "velocity": 0.5 }])",
                ReadingError::WrongTimeFormat }, // missing time
            { R"([{ "time": -1.0, "duration": 0.25, "number": 36, "velocity": 0.5 }])",
                ReadingError::WrongTimeFormat },
            { R"([{ "time": 0, "duration": 0.25, "number": 36, "velocity": 0.5 }])",
                ReadingError::WrongTimeFormat }, // times must be floating point
            { R"([{ "time": 0.0, "number": 36, "velocity": 0.5 }])",
                ReadingError::WrongNoteDuration }, // missing duration
            { R"([{ "time": 0.0, "duration": -0.25, "number": 36, "velocity": 0.5 }])",
                ReadingError::WrongNoteDuration },
            { R"([{ "time": 0.0, "duration": 0.25, "velocity": 0.5 }])",
                ReadingError::WrongNoteNumber }, // missing number
            { R"([{ "time": 0.0, "duration": 0.25, "number": 36.5, "velocity": 0.5 }])",
                ReadingError::WrongNoteNumber }, // fractional number
            { R"([{ "time": 0.0, "duration": 0.25, "number": 200, "velocity": 0.5 }])",
                ReadingError::WrongNoteNumber }, // out of range
            { R"([{ "time": 0.0, "duration": 0.25, "number": 36 }])",
                ReadingError::WrongNoteValue }, // missing velocity
            // The first bad note aborts the whole list
            { R"([{ "time": 0.0, "duration": 0.25, "number": 36, "velocity": 0.5 },
                  { "time": 1.0, "duration": 0.25, "number": 36 }])",
                ReadingError::WrongNoteValue },
        }));

        CAPTURE( notes );
        const auto f = read(("{\"notes\": " + notes + "}").c_str());
        REQUIRE( !f.has_value() );
        REQUIRE( f.error() == error );
    }
}
