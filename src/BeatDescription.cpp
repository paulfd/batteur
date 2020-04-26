#include "BeatDescription.h"
#include "MathHelpers.h"
#include <fmidi/fmidi.h>
#include "tl/expected.hpp"

using nlohmann::json;

namespace midi {
constexpr uint8_t statusMask { 0b11110000 };
constexpr uint8_t channelMask { 0b00001111 };
constexpr uint8_t noteOff { 0x80 };
constexpr uint8_t noteOn { 0x90 };
constexpr uint8_t polyphonicPressure { 0xA0 };
constexpr uint8_t controlChange { 0xB0 };
constexpr uint8_t programChange { 0xC0 };
constexpr uint8_t channelPressure { 0xD0 };
constexpr uint8_t pitchBend { 0xE0 };
constexpr uint8_t systemMessage { 0xF0 };

constexpr uint8_t status(uint8_t midiStatusByte)
{
    return midiStatusByte & statusMask;
}
constexpr uint8_t channel(uint8_t midiStatusByte)
{
    return midiStatusByte & channelMask;
}

constexpr int buildAndCenterPitch(uint8_t firstByte, uint8_t secondByte)
{
    return (int)(((unsigned int)secondByte << 7) + (unsigned int)firstByte) - 8192;
}

}

namespace { // anonymous namespace

struct BeatDescriptionErrorCategory : std::error_category {
    const char* name() const noexcept override;
    std::string message(int ev) const override;
};

const char* BeatDescriptionErrorCategory::name() const noexcept
{
    return "beat-descriptions";
}

std::string BeatDescriptionErrorCategory::message(int ev) const
{
    switch (static_cast<batteur::BeatDescriptionError>(ev)) {
    case batteur::BeatDescriptionError::NonexistentFile:
        return "File does not exist";

    case batteur::BeatDescriptionError::NoFilename:
        return "No filename key in the JSON dictionary";

    case batteur::BeatDescriptionError::NoParts:
        return "No parts found in the JSON dictionary";

    default:
        return "Unknown error";
    }
}

// https://akrzemi1.wordpress.com/2017/07/12/your-own-error-code/
const BeatDescriptionErrorCategory beatDescriptionErrorCategory {};

}

std::error_code batteur::make_error_code(batteur::BeatDescriptionError e)
{
  return { static_cast<int>(e), beatDescriptionErrorCategory };
}

tl::optional<unsigned> getQuarterPerBars(const fmidi_event_t& evt)
{
    if (evt.data[0] != 0x58)
        return {};

    return evt.data[1] / (1 << (evt.data[2] - 2));
}

tl::optional<double> getSecondsPerQuarter(const fmidi_event_t& evt)
{
    if (evt.data[0] != 0x51 || evt.datalen != 4)
        return {};
        
    const uint8_t* d24 = &evt.data[1];
    const uint32_t tempo = (d24[0] << 16) | (d24[1] << 8) | d24[2];
    return 1e-6 * tempo;
}

enum class MidiFileError{
    NotPresent,
    NoFilename,
    MidiFileError,
    WrongIgnoreBars,
    NoDataRead
};

// Note that the passed file should not be const
tl::expected<batteur::Sequence, MidiFileError> readMidiFile(nlohmann::json& json, const fs::path& rootDirectory)
{
    if (json.is_null())
        return tl::make_unexpected(MidiFileError::NotPresent);

    if (!json.contains("filename"))
        return tl::make_unexpected(MidiFileError::NoFilename);

    fs::path filepath = rootDirectory / json["filename"].get<std::string>();

    fmidi_smf_u midiFile { fmidi_smf_file_read(filepath.c_str()) };
    if (!midiFile) {
        DBG("Cannot read file {} from root directory {}", json["filename"].get<std::string>(), rootDirectory.native());
        return {};
    }
    batteur::Sequence returned;

    const auto v = json["ignore_bars"];
    if (!v.is_null() && !v.is_number_unsigned())
        return tl::make_unexpected(MidiFileError::WrongIgnoreBars);

    const auto ignoreBars = v.is_null() ? 0 : v.get<unsigned>();

    const auto findLastNoteOn = [&returned](uint8_t number, double time) -> void {
        for (auto it = returned.rbegin(); it != returned.rend(); ++it) {
            if (it->number == number) {
                it->duration = max(0.0, time - it->timestamp);
                return;
            }
        }
    };

    fmidi_seq_u midiSequencer { fmidi_seq_new(midiFile.get()) };
    fmidi_seq_event_t event;
    unsigned quarterPerBars { 4 };
    double secondsPerQuarter { 0.5 };
    auto ignoredQuarters = static_cast<double>(quarterPerBars * ignoreBars);
    while (fmidi_seq_next_event(midiSequencer.get(), &event)) {
        const auto& evt = event.event;
        switch (evt->type) {
        case (fmidi_event_meta):
            if (auto qpb = getQuarterPerBars(*evt)) {
                quarterPerBars = *qpb;
                ignoredQuarters = static_cast<double>(quarterPerBars * ignoreBars);
            } else if (auto spq = getSecondsPerQuarter(*evt)) {
                secondsPerQuarter = *spq;
            }
            break;
        case (fmidi_event_message):
            // go to the next process step
            break;
        default:
            // Ignore other messages
            continue;
        }

        const double timeInQuarters = event.time / secondsPerQuarter;
        if (timeInQuarters < ignoredQuarters)
            continue;

        switch (midi::status(evt->data[0])) {
        case midi::noteOff:
            findLastNoteOn(evt->data[1], timeInQuarters);
            break;
        case midi::noteOn:
            // It's a note-off
            if (evt->data[2] == 0) {
                findLastNoteOn(evt->data[1], timeInQuarters);
                break;
            }

            // It's a real note-on
            returned.push_back({ timeInQuarters, 0.0, evt->data[1], evt->data[2] });
            break;
        default:
            break;
        }
    }

    if (returned.empty())
        return tl::make_unexpected(MidiFileError::NoDataRead);

    for (auto& note : returned) {
        note.timestamp -= ignoredQuarters;
    }

#if 0
    DBG("Note NUM: TIME (DURATION)");
    for (auto& note : returned) {
        DBG("Note {} : {:.2f} ({:.2f})", +note.number, note.timestamp, note.duration );
    }
#endif
    return returned;
}

enum class BPMError{
    NotPresent,
    NotANumber,
    Negative
};

tl::expected<double, BPMError> checkBPM(const nlohmann::json& bpm)
{
    if (bpm.is_null())
        return tl::make_unexpected(BPMError::NotPresent);

    if (!bpm.is_number())
        return tl::make_unexpected(BPMError::NotANumber);
    
    const auto b = bpm.get<double>();

    if (b <= 0.0)
        return tl::make_unexpected(BPMError::Negative);

    return b;  
}

enum class QuarterPerBarsError{
    NotPresent,
    NotAnUnsigned,
    Zero
};

tl::expected<unsigned, QuarterPerBarsError> checkQuartersPerBar(const nlohmann::json& qpb)
{
    if (qpb.is_null())
        return tl::make_unexpected(QuarterPerBarsError::NotPresent);

    if (!qpb.is_number_unsigned())
        return tl::make_unexpected(QuarterPerBarsError::NotAnUnsigned);
    
    const auto q = qpb.get<unsigned>();

    if (q == 0)
        return tl::make_unexpected(QuarterPerBarsError::Zero);

    return q;  
}

std::unique_ptr<batteur::BeatDescription> batteur::BeatDescription::buildFromFile(const fs::path& file, std::error_code& error)
{
    if (!fs::exists(file)) {
        error = BeatDescriptionError::NonexistentFile;
        return {};
    }

    fs::fstream inputStream { file, std::ios::ios_base::in };
    nlohmann::json json;
    inputStream >> json;
    // DBG(json.dump(2));

    auto beat = std::unique_ptr<BeatDescription>(new BeatDescription());

    // Minimal file
    const auto title = json["name"];
    if (title.is_null()) {
        error = BeatDescriptionError::NoFilename;
        return {};
    }
    beat->name = title;

    const auto group = json["group"];
    if (!group.is_null())
        beat->group = group;

    auto parts = json["parts"];
    if (!parts.is_array() || parts.size() == 0) {
        error = BeatDescriptionError::NoParts;
        return {};
    }

    beat->bpm = checkBPM(json["bpm"]).value_or(120.0);
    beat->quartersPerBar = checkQuartersPerBar(json["quarters_per_bar"]).value_or(4);

    const auto rootDirectory = file.parent_path();

    if (auto seq = readMidiFile(json["intro"], rootDirectory))
        beat->intro = std::move(*seq);

    if (auto seq = readMidiFile(json["ending"], rootDirectory))
        beat->ending = std::move(*seq);

    for (auto& part : parts) {
        Part newPart;
        newPart.name = part["name"];
        auto mainLoop = readMidiFile(part["midi_file"], rootDirectory);
        
        if (!mainLoop)
            continue;

        newPart.mainLoop = std::move(*mainLoop);

        for (auto& fill : part["fills"]) {
            if (auto seq = readMidiFile(fill, rootDirectory))
                newPart.fills.push_back(std::move(*seq));
        }

        if (auto seq = readMidiFile(json["transition"], rootDirectory))
            newPart.transition = std::move(*seq);

        beat->parts.push_back(std::move(newPart));
    }

    if (beat->parts.empty()) {
        error = BeatDescriptionError::NoParts;
        return {};
    }


    DBG("File: {}", file.native());
    DBG("Name: {}", beat->name);
    DBG("Group: {}", beat->group);
    DBG("BPM: {}", beat->bpm);
    return beat;
}
