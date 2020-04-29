#pragma once
#include "json.hpp"
#include "tl/expected.hpp"
#include "tl/optional.hpp"
#include "BeatDescription.h"
#include "fmidi/fmidi.h"

enum class MidiFileError {
    NotPresent,
    NoFilename,
    MidiFileError,
    WrongIgnoreBars,
    WrongBars,
    NoDataRead
};

enum class BPMError {
    NotPresent,
    NotANumber,
    Negative
};

enum class QuarterPerBarsError {
    NotPresent,
    NotAnUnsigned,
    Zero
};

// Helper functions

tl::expected<batteur::Sequence, MidiFileError> readMidiFile(nlohmann::json& json, const fs::path& rootDirectory);
tl::expected<double, BPMError> checkBPM(const nlohmann::json& bpm);
tl::expected<unsigned, QuarterPerBarsError> checkQuartersPerBar(const nlohmann::json& qpb);
double getNumBars(const batteur::Sequence& sequence, unsigned quartersPerBar);
void alignSequenceEnd(batteur::Sequence& sequence, double numBars, unsigned quartersPerBar);
tl::optional<unsigned> getQuarterPerBars(const fmidi_event_t& evt);
tl::optional<double> getSecondsPerQuarter(const fmidi_event_t& evt);