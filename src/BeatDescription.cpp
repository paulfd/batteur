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
  
double barCount(const Sequence& sequence, unsigned quartersPerBar)
{
    if (sequence.empty())
        return 0.0;

    return std::ceil(sequence.back().timestamp / quartersPerBar);
}

void alignSequenceEnd(Sequence& sequence, double numBars, unsigned quartersPerBar)
{
    const double sequenceBars = barCount(sequence, quartersPerBar);
    DBG("Number of bars in the sequence to align" << sequenceBars
        << "vs the one to align to" << numBars);
    const double shift = (numBars - sequenceBars) * quartersPerBar;
    DBG("Shifting by " << shift);

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

nlohmann::json serializeNotes(const Sequence& sequence)
{
    nlohmann::json json;
    for (const auto& note: sequence) {
        nlohmann::json n;
        n["time"] = note.timestamp;
        n["duration"] = note.timestamp;
        n["number"] = note.number;
        n["velocity"] = note.velocity;
        json["notes"].push_back(std::move(n));
    }
    return json;
}

nlohmann::json serializePart(const Part& part)
{
    nlohmann::json json;
    json["name"] = part.name;
    json["sequence"] = serializeNotes(part.mainLoop);
    for (const auto& fill: part.fills) {
        json["fills"].push_back(serializeNotes(fill));
    }

    if (part.transition && !part.transition->empty())
        json["transition"] = serializeNotes(*part.transition);

    return json;
}

std::string BeatDescription::saveMonolithic()
{
    nlohmann::json json;
    json["name"] = name;
    if (group != "")
        json["group"] = group;
    
    json["bpm"] = bpm;
    json["quarters_per_bar"] = quartersPerBar;

    if (intro && !intro->empty())
        json["intro"] = serializeNotes(*intro);

    if (ending && !ending->empty())
        json["ending"] = serializeNotes(*ending);

    for (const auto& part: parts)
        json["parts"].push_back(serializePart(part));

    return json.dump(2);
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

    if (auto seq = readSequence(json["intro"], rootDirectory))
        beat->intro = std::move(*seq);

    if (auto seq = readSequence(json["ending"], rootDirectory))
        beat->ending = std::move(*seq);

    for (auto& part : parts) {
        Part newPart;
        newPart.name = part["name"];
        auto mainLoop = readSequence(part["sequence"], rootDirectory);
        if (!mainLoop)
            continue;

        newPart.mainLoop = std::move(*mainLoop);

        for (auto& fill : part["fills"]) {
            if (auto seq = readSequence(fill, rootDirectory)) {
                newPart.fills.push_back(std::move(*seq));
            }
        }

        if (auto seq = readSequence(part["transition"], rootDirectory)) {
            newPart.transition = std::move(*seq);
        }
        beat->parts.push_back(std::move(newPart));
    }

    if (beat->parts.empty()) {
        error = BeatDescriptionError::NoParts;
        return {};
    }

    return beat;
}
  
}