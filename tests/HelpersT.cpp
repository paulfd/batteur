#include "BeatDescription.h"
#include "FileReadingHelpers.h"
#include "MathHelpers.h"
#include "MidiHelpers.h"
#include "catch.hpp"
#include <cstddef>

using namespace Catch::literals;
using namespace batteur;

namespace {

/**
 * @brief Builds a standalone meta event, since fmidi_event_t has a trailing
 *        flexible data array and cannot simply be stack-allocated.
 */
class MetaEvent {
public:
    explicit MetaEvent(const std::vector<uint8_t>& data)
        : storage(fmidi_event_sizeof(data.size()))
    {
        auto* evt = reinterpret_cast<fmidi_event_t*>(storage.data());
        evt->type = fmidi_event_meta;
        evt->delta = 0;
        evt->datalen = static_cast<uint32_t>(data.size());
        std::copy(data.begin(), data.end(), evt->data);
    }

    const fmidi_event_t& operator*() const
    {
        return *reinterpret_cast<const fmidi_event_t*>(storage.data());
    }

private:
    std::vector<uint8_t> storage;
};

Sequence sequenceAt(std::initializer_list<double> timestamps)
{
    Sequence sequence;
    for (double timestamp : timestamps)
        sequence.emplace_back(timestamp, 0.25, 36, 0.8f);
    return sequence;
}

}

TEST_CASE("[Math] Bounds helpers")
{
    // The variadic overloads fold over the binary ones
    REQUIRE( max(1, 4, 2) == 4 );
    REQUIRE( max(5, 1, 2, 3) == 5 );
    REQUIRE( min(4, 1, 2) == 1 );
    REQUIRE( min(5, 4, 3, 2) == 2 );
    REQUIRE( max(-3.5, 1.5) == 1.5 );
    REQUIRE( min(-3.5, 1.5) == -3.5 );

    REQUIRE( clamp(0.5f, 0.0f, 1.0f) == 0.5f );
    REQUIRE( clamp(-0.5f, 0.0f, 1.0f) == 0.0f );
    REQUIRE( clamp(1.5f, 0.0f, 1.0f) == 1.0f );

    // Both helpers include their bounds
    REQUIRE( clamp(0.0f, 0.0f, 1.0f) == 0.0f );
    REQUIRE( clamp(1.0f, 0.0f, 1.0f) == 1.0f );
    REQUIRE( within(0, 0, 10) );
    REQUIRE( within(10, 0, 10) );
    REQUIRE( !within(-1, 0, 10) );
    REQUIRE( !within(11, 0, 10) );
}

TEST_CASE("[Midi] Status bytes")
{
    // The status and the channel are carved out of the same byte
    REQUIRE( midi::status(0x94) == midi::noteOn );
    REQUIRE( midi::channel(0x94) == 4 );
    REQUIRE( midi::status(0x8F) == midi::noteOff );
    REQUIRE( midi::channel(0x8F) == 15 );

    // Pitch bends are 14 bits, little endian, centered on 0x2000
    REQUIRE( midi::buildAndCenterPitch(0x00, 0x40) == 0 );
    REQUIRE( midi::buildAndCenterPitch(0x00, 0x00) == -8192 );
    REQUIRE( midi::buildAndCenterPitch(0x7F, 0x7F) == 8191 );
}

TEST_CASE("[Midi] Time signature meta event")
{
    double quartersPerBar;
    std::vector<uint8_t> data;
    std::tie(data, quartersPerBar) = GENERATE(table<std::vector<uint8_t>, double>({
        { { 0x58, 4, 2, 24, 8 }, 4.0 },
        { { 0x58, 12, 3, 24, 8 }, 6.0 },
        { { 0x58, 7, 3, 24, 8 }, 3.5 },
        { { 0x58, 5, 4, 24, 8 }, 1.25 },
    }));

    const MetaEvent evt { data };
    REQUIRE( getQuarterPerBars(*evt) );
    REQUIRE( *getQuarterPerBars(*evt) == Approx(quartersPerBar) );

    // A tempo event is not a time signature
    const MetaEvent tempo {{ 0x51, 0x07, 0xA1, 0x20 }};
    REQUIRE( !getQuarterPerBars(*tempo) );
}

TEST_CASE("[Midi] Tempo meta event")
{
    SECTION("Microseconds per quarter are converted to seconds")
    {
        double secondsPerQuarter;
        std::vector<uint8_t> data;
        std::tie(data, secondsPerQuarter) = GENERATE(table<std::vector<uint8_t>, double>({
            { { 0x51, 0x07, 0xA1, 0x20 }, 0.5 }, // 120 bpm
            { { 0x51, 0x0F, 0x42, 0x40 }, 1.0 }, // 60 bpm
        }));

        const MetaEvent evt { data };
        REQUIRE( getSecondsPerQuarter(*evt) );
        REQUIRE( *getSecondsPerQuarter(*evt) == Approx(secondsPerQuarter) );
    }

    SECTION("Not a tempo event")
    {
        const MetaEvent evt {{ 0x58, 4, 2, 24, 8 }};
        REQUIRE( !getSecondsPerQuarter(*evt) );
    }

    SECTION("Truncated tempo event")
    {
        const MetaEvent evt {{ 0x51, 0x07, 0xA1 }};
        REQUIRE( !getSecondsPerQuarter(*evt) );
    }
}

TEST_CASE("[Sequences] barCount")
{
    REQUIRE( barCount(Sequence {}, 4.0) == 0.0 );
    // A sequence that ends right before the bar line still holds a single bar
    REQUIRE( barCount(sequenceAt({ 0.0, 1.0, 2.0, 3.0 }), 4.0) == 1.0 );
    REQUIRE( barCount(sequenceAt({ 0.0, 3.5 }), 4.0) == 1.0 );
    // A note exactly on the bar line belongs to the bar that just ended
    REQUIRE( barCount(sequenceAt({ 0.0, 4.0 }), 4.0) == 1.0 );
    REQUIRE( barCount(sequenceAt({ 0.0, 4.5 }), 4.0) == 2.0 );
    REQUIRE( barCount(sequenceAt({ 0.0, 7.5 }), 4.0) == 2.0 );
    // Only the last timestamp matters
    REQUIRE( barCount(sequenceAt({ 0.0, 20.0, 1.0 }), 4.0) == 1.0 );

    // Odd meters
    REQUIRE( barCount(sequenceAt({ 0.0, 3.0 }), 3.5) == 1.0 );
    REQUIRE( barCount(sequenceAt({ 0.0, 4.0 }), 3.5) == 2.0 );
    REQUIRE( barCount(sequenceAt({ 0.0, 5.0 }), 6.0) == 1.0 );
}

TEST_CASE("[Sequences] alignSequenceEnd")
{
    SECTION("Shorter sequences are pushed back")
    {
        // A one-bar sequence aligned on the end of a two-bar one gets pushed
        // back by a full bar
        auto sequence = sequenceAt({ 0.0, 1.0, 2.0, 3.0 });
        alignSequenceEnd(sequence, 2.0, 4.0);
        REQUIRE( sequence.size() == 4 );
        REQUIRE( sequence[0].timestamp == 4.0_a );
        REQUIRE( sequence[3].timestamp == 7.0_a );
        REQUIRE( barCount(sequence, 4.0) == 2.0 );
    }

    SECTION("Matching sequences are left alone")
    {
        auto sequence = sequenceAt({ 0.0, 1.0, 2.0, 3.0 });
        alignSequenceEnd(sequence, 1.0, 4.0);
        REQUIRE( sequence.size() == 4 );
        REQUIRE( sequence[0].timestamp == 0.0_a );
        REQUIRE( sequence[3].timestamp == 3.0_a );
    }

    SECTION("Longer sequences are truncated from the front")
    {
        // A two-bar sequence aligned on a one-bar one should drop the first
        // bar and keep the second one in place.
        auto sequence = sequenceAt({ 0.0, 1.0, 4.0, 5.0 });
        alignSequenceEnd(sequence, 1.0, 4.0);
        REQUIRE( sequence.size() == 2 );
        REQUIRE( sequence[0].timestamp == 0.0_a );
        REQUIRE( sequence[1].timestamp == 1.0_a );
    }

    SECTION("An empty sequence stays empty")
    {
        Sequence sequence;
        alignSequenceEnd(sequence, 2.0, 4.0);
        REQUIRE( sequence.empty() );
    }
}

TEST_CASE("[Errors] Beat description error codes")
{
    const std::error_code nonexistent = BeatDescriptionError::NonexistentFile;
    REQUIRE( nonexistent );
    REQUIRE( std::string(nonexistent.category().name()) == "beat-descriptions" );
    REQUIRE( !nonexistent.message().empty() );

    // Values outside the enum fall back rather than reading past the switch
    const auto unknown = make_error_code(static_cast<BeatDescriptionError>(4242));
    REQUIRE( unknown.message() == "Unknown error" );

    // Distinct errors do not compare equal
    REQUIRE( nonexistent != std::error_code(BeatDescriptionError::NoParts) );
    REQUIRE( nonexistent == BeatDescriptionError::NonexistentFile );
}
