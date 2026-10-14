#include "BeatDescription.h"
#include "catch.hpp"
#include <sstream>
#include <string>

using namespace Catch::literals;
using namespace batteur;

namespace {

fs::path testFile(const char* name)
{
    return fs::current_path() / "tests/files" / name;
}

/// A single note, enough for a sequence to be read back successfully
const char* oneNote = R"({ "notes": [
    { "time": 0.0, "duration": 0.25, "number": 36, "velocity": 0.5 } ] })";

/// The smallest description that loads, plus whatever extra keys are needed.
/// `extraKeys` is appended as written, so it starts with a comma.
std::unique_ptr<BeatDescription> buildWith(const std::string& extraKeys = "")
{
    std::error_code ec;
    const std::string text = std::string(R"({ "name": "Test", "parts": [ { "name": "Only", "sequence": )")
        + oneNote + " } ]" + extraKeys + " }";
    return BeatDescription::buildFromString("virtual.json", text, ec);
}

}

TEST_CASE("[Files] Files that cannot be opened")
{
    const auto path = GENERATE(as<std::string> {}, "", "fake_file.json");
    CAPTURE( path );

    std::error_code ec;
    REQUIRE( !BeatDescription::buildFromFile(path, ec) );
    REQUIRE( ec == BeatDescriptionError::NonexistentFile );
}

TEST_CASE("[Files] The whole description is read back")
{
    std::error_code ec;
    auto beat = BeatDescription::buildFromFile(testFile("shuffle.json"), ec);
    REQUIRE( beat );
    REQUIRE( !ec );
    REQUIRE( beat->name == "Slow shuffle" );
    REQUIRE( beat->group == "Blues" );
    REQUIRE( beat->bpm == 78 );
    REQUIRE( beat->quartersPerBar == 4.0 );
    REQUIRE( beat->intro );
    REQUIRE( !beat->intro->empty() );
    REQUIRE( beat->parts.size() == 2 );
    REQUIRE( beat->parts[0].name == "Snare" );
    REQUIRE( beat->parts[1].name == "Ride" );
    REQUIRE( beat->parts[0].fills.size() == 2 );
    REQUIRE( beat->parts[1].fills.size() == 1 );
    REQUIRE( beat->parts[0].transition );
    REQUIRE( !beat->parts[0].mainLoop.empty() );
    for (const auto& fill : beat->parts[0].fills)
        REQUIRE( !fill.empty() );
}

TEST_CASE("[Files] Reading from a string matches reading from the file")
{
    // buildFromString resolves the relative MIDI filenames against the
    // virtual path it is handed, so both paths must agree on the same text.
    const auto path = testFile("shuffle.json");
    fs::ifstream stream { path };
    std::stringstream buffer;
    buffer << stream.rdbuf();

    std::error_code fromStringError;
    std::error_code fromFileError;
    auto fromString = BeatDescription::buildFromString(path, buffer.str(), fromStringError);
    auto fromFile = BeatDescription::buildFromFile(path, fromFileError);
    REQUIRE( fromString );
    REQUIRE( fromFile );

    REQUIRE( fromString->name == fromFile->name );
    REQUIRE( fromString->bpm == fromFile->bpm );
    REQUIRE( fromString->quartersPerBar == fromFile->quartersPerBar );
    REQUIRE( fromString->parts.size() == fromFile->parts.size() );
    for (size_t i = 0; i < fromFile->parts.size(); ++i) {
        REQUIRE( fromString->parts[i].name == fromFile->parts[i].name );
        REQUIRE( fromString->parts[i].mainLoop.size() == fromFile->parts[i].mainLoop.size() );
        REQUIRE( fromString->parts[i].fills.size() == fromFile->parts[i].fills.size() );
    }
}

TEST_CASE("[Files] Sequences can be spelled out as note lists")
{
    std::error_code ec;
    const std::string text = std::string(R"({
        "name": "Inline",
        "bpm": 120.0,
        "intro": )") + oneNote + R"(,
        "ending": )" + oneNote + R"(,
        "parts": [
            { "name": "Hat", "sequence": )" + oneNote + R"(,
              "fills": [ )" + oneNote + ", " + oneNote + R"( ],
              "transition": )" + oneNote + R"( },
            { "name": "Ride", "sequence": )" + oneNote + R"(,
              "fills": [ )" + oneNote + R"( ],
              "transition": )" + oneNote + R"( }
        ]
    })";

    auto beat = BeatDescription::buildFromString("virtual.json", text, ec);
    REQUIRE( beat );
    REQUIRE( beat->bpm == 120 );
    REQUIRE( beat->intro );
    REQUIRE( beat->ending );
    REQUIRE( beat->parts.size() == 2 );
    REQUIRE( beat->parts[0].name == "Hat" );
    REQUIRE( beat->parts[1].name == "Ride" );
    REQUIRE( beat->parts[0].fills.size() == 2 );
    REQUIRE( beat->parts[1].fills.size() == 1 );
    REQUIRE( beat->parts[0].transition );
    REQUIRE( beat->parts[1].transition );
}

TEST_CASE("[Files] Time signatures")
{
    std::string file;
    double quartersPerBar;
    int num;
    int denom;
    std::tie(file, quartersPerBar, num, denom) = GENERATE(table<std::string, double, int, int>({
        { "sig1.json", 4.0, 4, 4 },
        { "sig2.json", 6.0, 12, 8 },
        { "sig3.json", 3.5, 7, 8 },
    }));

    CAPTURE( file );
    std::error_code ec;
    auto beat = BeatDescription::buildFromFile(testFile(file.c_str()), ec);
    REQUIRE( beat );
    REQUIRE( beat->quartersPerBar == Approx(quartersPerBar) );
    REQUIRE( beat->signature.num == num );
    REQUIRE( beat->signature.denom == denom );
}

TEST_CASE("[Files] Descriptions without a usable name are rejected")
{
    std::error_code ec;
    const auto text = GENERATE(as<std::string> {},
        R"({ "name": null, "parts": [] })",
        "", // not even JSON
        "{",
        "[1, 2, 3]", // JSON, but not an object
        "{}");

    CAPTURE( text );
    REQUIRE( !BeatDescription::buildFromString("virtual.json", text, ec) );
    REQUIRE( ec == BeatDescriptionError::NoFilename );
}

TEST_CASE("[Files] Descriptions without usable parts are rejected")
{
    std::error_code ec;
    const auto text = GENERATE(as<std::string> {},
        R"({ "name": "Test" })", // no parts key
        R"({ "name": "Test", "parts": [] })",
        R"({ "name": "Test", "parts": { "name": "Nope" } })", // not an array
        R"({ "name": "Test", "parts": [ { "name": "Nope", "sequence": {} } ] })");

    CAPTURE( text );
    REQUIRE( !BeatDescription::buildFromString("virtual.json", text, ec) );
    REQUIRE( ec == BeatDescriptionError::NoParts );
}

TEST_CASE("[Files] Parts that cannot be read are skipped")
{
    std::error_code ec;
    const std::string text = std::string(R"({
        "name": "Test",
        "parts": [
            { "name": "Broken", "sequence": { "filename": "nope.mid" } },
            { "name": "Good", "sequence": )") + oneNote + R"( }
        ]
    })";

    auto beat = BeatDescription::buildFromString("virtual.json", text, ec);
    REQUIRE( beat );
    REQUIRE( beat->parts.size() == 1 );
    REQUIRE( beat->parts[0].name == "Good" );
}

TEST_CASE("[Files] Defaults")
{
    auto beat = buildWith();
    REQUIRE( beat );
    REQUIRE( beat->group.empty() );
    REQUIRE( beat->bpm == 120.0_a );
    REQUIRE( beat->quartersPerBar == 4.0 );
    REQUIRE( beat->signature.num == 4 );
    REQUIRE( beat->signature.denom == 4 );
    REQUIRE( !beat->intro );
    REQUIRE( !beat->ending );

    SECTION("Parts may omit their optional keys")
    {
        std::error_code ec;
        const std::string text = std::string(R"({ "name": "Test", "parts": [ { "sequence": )")
            + oneNote + " } ] }";
        auto anonymous = BeatDescription::buildFromString("virtual.json", text, ec);
        REQUIRE( anonymous );
        REQUIRE( anonymous->parts.size() == 1 );
        REQUIRE( anonymous->parts[0].name.empty() );
        REQUIRE( anonymous->parts[0].fills.empty() );
        REQUIRE( !anonymous->parts[0].transition );
    }
}

TEST_CASE("[Files] Malformed meters fall back to 4/4")
{
    // A meter is one value: a valid half must never be combined with a
    // default, and a zero denominator must not be divided by.
    const auto extra = GENERATE(as<std::string> {},
        R"(, "quarters_per_bar": 0)",
        R"(, "quarters_per_bar": -3)",
        R"(, "quarters_per_bar": "three")",
        R"(, "signature": [7])", // too few entries
        R"(, "signature": "7/8")", // not an array
        R"(, "signature": [7.5, 8])", // non integer entries
        R"(, "signature": [7, 0])"); // zero denominator

    CAPTURE( extra );
    auto beat = buildWith(extra);
    REQUIRE( beat );
    REQUIRE( beat->quartersPerBar == 4.0 );
    REQUIRE( beat->signature.num == 4 );
    REQUIRE( beat->signature.denom == 4 );
}

TEST_CASE("[Files] Valid meters are read back")
{
    SECTION("From quarters_per_bar, which implies a denominator of four")
    {
        auto beat = buildWith(R"(, "quarters_per_bar": 3)");
        REQUIRE( beat );
        REQUIRE( beat->quartersPerBar == 3.0 );
        REQUIRE( beat->signature.num == 3 );
        REQUIRE( beat->signature.denom == 4 );
    }

    SECTION("From a signature, which implies the quarters per bar")
    {
        auto beat = buildWith(R"(, "signature": [5, 4])");
        REQUIRE( beat );
        REQUIRE( beat->quartersPerBar == 5.0 );
        REQUIRE( beat->signature.num == 5 );
        REQUIRE( beat->signature.denom == 4 );
    }

    SECTION("Quarters per bar takes precedence over the signature")
    {
        auto beat = buildWith(R"(, "quarters_per_bar": 3, "signature": [12, 8])");
        REQUIRE( beat );
        REQUIRE( beat->quartersPerBar == 3.0 );
        REQUIRE( beat->signature.num == 3 );
        REQUIRE( beat->signature.denom == 4 );
    }
}

TEST_CASE("[Files] Every shipped beat loads")
{
    const auto beatsDirectory = fs::current_path() / "beats";
    REQUIRE( fs::is_directory(beatsDirectory) );

    int loaded = 0;
    for (const auto& entry : fs::directory_iterator(beatsDirectory)) {
        if (entry.path().extension() != ".json")
            continue;

        std::error_code ec;
        INFO( "Loading " << entry.path().string() );
        auto beat = BeatDescription::buildFromFile(entry.path(), ec);
        REQUIRE( beat );
        REQUIRE( !ec );
        REQUIRE( !beat->name.empty() );
        REQUIRE( beat->bpm > 0.0f );
        REQUIRE( beat->quartersPerBar > 0.0 );
        REQUIRE( !beat->parts.empty() );
        for (const auto& part : beat->parts) {
            REQUIRE( !part.name.empty() );
            REQUIRE( !part.mainLoop.empty() );
        }
        loaded++;
    }

    REQUIRE( loaded > 0 );
}
