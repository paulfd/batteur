#include <iostream>
#include <fmt/core.h>
#include "BeatDescription.h"

void usage()
{
    std::cerr << "Usage: " << '\n';
    std::cerr << "\tbatteur-serialize BATTEUR_DESCRIPTION_FILE" << '\n';
}

static int paddingLevel { 0 };
static std::string paddingString { "  " };

static void pad()
{
    for (int i = 0; i < paddingLevel; ++i) {
        fmt::print(paddingString);
    }
}

static void printSequence(const batteur::Sequence& sequence)
{
    if (sequence.empty())
        return;

    fmt::print("{{\n");
    paddingLevel++;
    pad();
    
    fmt::print("\"notes\": [\n");
    paddingLevel++;
    pad();

    unsigned i { 0 };
    while (true) {
        const auto& note = sequence[i];
        fmt::print(
            "{{ \"time\": {:.4f}, \"duration\": {:.4f}, \"number\": {:3d}, \"velocity\": {:3d} }}",
            note.timestamp,
            note.duration,
            note.number,
            note.velocity
        );

        if (++i == sequence.size()) {
            fmt::print("\n");
            break;
        } else {
            fmt::print(",\n");
            pad();
        }
    }
    paddingLevel--;
    pad();
    fmt::print("]\n");
    paddingLevel--;
    pad();
    fmt::print("}}");
}

void printPart(const batteur::Part& part)
{
    fmt::print("{{\n");
    paddingLevel++;
    pad();
    fmt::print("\"name\": \"{}\",\n", part.name);
    pad();
    fmt::print("\"sequence\": ");
    printSequence(part.mainLoop);

    if (!part.fills.empty()) {
        fmt::print(",\n");
        pad();
        fmt::print("\"fills\": [\n");
        paddingLevel++;
        pad();

        unsigned i { 0 };
        while (true) {
            const auto& fill = part.fills[i];
            printSequence(fill);
            if (++i == part.fills.size()) {
                fmt::print("\n");
                paddingLevel--;
                pad();
                break;
            } else {
                fmt::print(",\n");
                pad();
            }
        }
        fmt::print("]");
    }
    
    
    if (part.transition) {
        fmt::print(",\n");
        pad();
        fmt::print("\"transition\": ");
        printSequence(*part.transition);
    }

    fmt::print("\n");
    paddingLevel--;
    pad();
    fmt::print("}}");
}

int main(int argc, char** argv)
{
    if (argc != 2) {
        usage();
        return -1;
    }

    std::error_code ec;
    auto beat = batteur::BeatDescription::buildFromFile(argv[1], ec);
    if (ec) {
        std::cerr << "Error reading the file " << argv[1] << " (" << ec << ")\n";
        return -1;
    }

    if (beat == nullptr) {
        std::cerr << "Unexpected error\n";
        return -1;
    }

    fmt::print("{{\n");
    paddingLevel++;
    pad();
    fmt::print("\"name\": \"{}\",\n", beat->name);
    pad();
    if (beat->group != "") {
        fmt::print("\"group\": \"{}\",\n", beat->group);
        pad();
    }
    fmt::print("\"bpm\": {},\n", beat->bpm); 
    pad();
    fmt::print("\"quarters_per_bar\": {}", beat->quartersPerBar);

    if (beat->intro) {
        fmt::print(",\n");
        pad();
        fmt::print("\"intro\": ");
        printSequence(*beat->intro);
    }

    if (beat->ending) {
        fmt::print(",\n");
        pad();
        fmt::print("\"ending\": ");
        printSequence(*beat->ending);
    }

    if (!beat->parts.empty()) {
        fmt::print(",\n");
        pad();
        fmt::print("\"parts\": [\n");
        paddingLevel++;
        pad();

        unsigned i { 0 };
        while (true) {
            const auto& part = beat->parts[i];
            printPart(part);
            if (++i == beat->parts.size()) {
                fmt::print("\n");
                paddingLevel--;
                pad();
                break;
            } else {
                fmt::print(",\n");
                pad();
            }
        }

        fmt::print("]\n");
        paddingLevel--;
        pad();
    }

    fmt::print("}}\n");

    return 0;
}