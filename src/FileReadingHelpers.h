#pragma once
#include "json.hpp"
#include "tl/expected.hpp"
#include "tl/optional.hpp"
#include "BeatDescription.h"
#include "fmidi/fmidi.h"

enum class ReadingError {
    NotPresent,
    NoFilename,
    MidiFileError,
    WrongIgnoreBars,
    WrongBars,
    ZeroBars,
    WrongNoteListFormat,
    WrongTimeFormat,
    WrongNoteDuration,
    WrongNoteNumber,
    WrongNoteValue,
    NoDataRead
};

enum class BPMError {
    NotPresent,
    NotANumber,
    Negative
};

enum class QuartersPerBarError {
    NotPresent,
    NotAnUnsigned,
    Zero
};

// Helper functions

tl::expected<batteur::Sequence, ReadingError> readSequence(nlohmann::json& json, const fs::path& rootDirectory);
tl::expected<double, BPMError> checkBPM(const nlohmann::json& bpm);
tl::expected<unsigned, QuartersPerBarError> checkQuartersPerBar(const nlohmann::json& qpb);

tl::optional<unsigned> getQuarterPerBars(const fmidi_event_t& evt);
tl::optional<double> getSecondsPerQuarter(const fmidi_event_t& evt);