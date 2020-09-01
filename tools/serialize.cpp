#include <iostream>
#include "BeatDescription.h"

void usage()
{
    std::cerr << "Usage: " << '\n';
    std::cerr << "\tbatteur-serialize BATTEUR_DESCRIPTION_FILE" << '\n';
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

    if (beat != nullptr)
        std::cout << beat->saveMonolithic();

    return 0;
}