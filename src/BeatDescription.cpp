#include "BeatDescription.h"
#include "MathHelpers.h"
#include <fmidi/fmidi.h>

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

template <class T>
constexpr float normalize7Bits(T value)
{
    return static_cast<float>(min(max(value, T { 0 }), T { 127 })) / 127.0f;
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

// Note that the passed file should not be const
tl::optional<batteur::Sequence> readMidiFile(nlohmann::json& json, const fs::path& rootDirectory)
{
    if (json.is_null()) {
        DBG("JSON data is null");
        return {};
    }

    if (!json.contains("filename")) {
        DBG("JSON does not contain a filename");
        return {};
    }

    fs::path filepath = rootDirectory / json["filename"].get<std::string>();

    fmidi_smf_u midiFile { fmidi_smf_file_read(filepath.c_str()) };
    if (!midiFile) {
        DBG("Cannot read file {} from root directory {}", json["filename"].get<std::string>(), rootDirectory.native());
        return {};
    }

    batteur::Sequence returned;
    const auto ignoreBars = [&]() -> int {
        const auto v = json["ignore_bars"];
        if (v.is_null() || !v.is_number())
            return 0;

        return max(0, v.get<int>());
    }();

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
    int quarterPerBars { 4 };
    double secondsPerQuarter { 0.5 };
    auto ignoredQuarters = static_cast<double>(quarterPerBars * ignoreBars);
    while (fmidi_seq_next_event(midiSequencer.get(), &event)) {
        const auto& evt = event.event;
        switch (evt->type) {
        case (fmidi_event_meta):
            if (evt->data[0] == 0x58) { // Set quarter per bars
                quarterPerBars = evt->data[1] / (1 << (evt->data[2] - 2));
                ignoredQuarters = static_cast<double>(quarterPerBars * ignoreBars);
            } else if (evt->data[0] == 0x51 && evt->datalen == 4) { // set tempo
                const uint8_t* d24 = &evt->data[1];
                const uint32_t tempo = (d24[0] << 16) | (d24[1] << 8) | d24[2];
                secondsPerQuarter = 1e-6 * tempo;
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

    const auto bpm = json["bpm"];
    if (!bpm.is_null() && bpm.is_number()) {
        auto b = bpm.get<float>();
        beat->bpm = clamp(b, 20.0f, 300.0f);
    } else {
        beat->bpm = 120.0f;
    }

    const auto qpb = json["quarters_per_bar"];
    if (!qpb.is_null() && qpb.is_number_integer()) {
        auto b = qpb.get<int>();
        beat->quartersPerBar = clamp(b, 1, 12);
    } else {
        beat->quartersPerBar = 4;
    }

    const auto rootDirectory = file.parent_path();
    beat->intro = readMidiFile(json["intro"], rootDirectory);
    beat->ending = readMidiFile(json["ending"], rootDirectory);

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
        newPart.transition = readMidiFile(part["transition"], rootDirectory);

        beat->parts.push_back(std::move(newPart));
    }

    DBG("File: {}", file.native());
    DBG("Name: {}", beat->name);
    DBG("Group: {}", beat->group);
    DBG("BPM: {}", beat->bpm);
    return beat;
}
