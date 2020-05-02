#pragma once
#include <vector>
#include <string>
#include <fstream>
#include "filesystem.hpp"
#include "Debug.h"
#include "tl/optional.hpp"

namespace fs = ghc::filesystem;

namespace batteur {

struct Note {
    double timestamp;
    double duration;
    uint8_t number;
    uint8_t velocity;
};

using Sequence = std::vector<Note>;

double barCount(const Sequence& sequence, unsigned quartersPerBar);
void alignSequenceEnd(Sequence& sequence, double numBars, unsigned quartersPerBar);
unsigned quarterStart(const Sequence& sequence, unsigned quartersPerBar);
unsigned quarterEnd(const Sequence& sequence, unsigned quartersPerBar);

struct Part {
    std::string name;
    Sequence mainLoop;
    std::vector<Sequence> fills;
    tl::optional<Sequence> transition;
};


struct BeatDescription {
    std::string name;
    std::string group;
    float bpm;
    unsigned quartersPerBar;
    tl::optional<Sequence> intro;
    std::vector<Part> parts;
    tl::optional<Sequence> ending;
    static std::unique_ptr<BeatDescription> buildFromFile(const fs::path& file, std::error_code& error);
};

enum class BeatDescriptionError {
    NonexistentFile = 1,
    NoFilename,
    NoParts
};

std::error_code make_error_code(BeatDescriptionError);

}

template <>
struct std::is_error_code_enum<batteur::BeatDescriptionError> : true_type {};
