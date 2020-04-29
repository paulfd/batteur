#include "BeatDescription.h"
#include "MathHelpers.h"
#include <fmidi/fmidi.h>
#include "tl/expected.hpp"
#include "FileReadingHelpers.h"
#include "json.hpp"

using nlohmann::json;

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

namespace batteur{
  
double getNumBars(const Sequence& sequence, unsigned quartersPerBar)
{
    if (sequence.empty())
        return 0.0;

    return std::ceil(sequence.back().timestamp / quartersPerBar);
}

void alignSequenceEnd(Sequence& sequence, double numBars, unsigned quartersPerBar)
{
    const double sequenceBars = getNumBars(sequence, quartersPerBar);
    DBG("Number of bars in the sequence to align {:.2f} vs the one to align to {:.1f}", sequenceBars, numBars);
    const double shift = (numBars - sequenceBars) * quartersPerBar;
    DBG("Shifting by {:.2f}", shift);

    if (shift < 0.0) {
        sequence.erase(
            sequence.begin(),
            std::find_if(
                sequence.begin(),
                sequence.end(),
                [shift](const Note& note) { return note.timestamp >= shift; }));
    }

    for (auto& note : sequence)
        note.timestamp += shift;
}

std::unique_ptr<BeatDescription> BeatDescription::buildFromFile(const fs::path& file, std::error_code& error)
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
            if (auto seq = readMidiFile(fill, rootDirectory)) {
                newPart.fills.push_back(std::move(*seq));
            }
        }

        if (auto seq = readMidiFile(json["transition"], rootDirectory)) {
            newPart.transition = std::move(*seq);
        }

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
  
}