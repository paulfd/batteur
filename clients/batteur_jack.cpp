#include <jack/jack.h>
#include "cxxopts.hpp"
#include "batteur.h"

struct batteur_beat_deleter {
    void operator()(batteur_beat_t* x) const { batteur_free_beat(x); }
};

using batteur_beat_u = std::unique_ptr<batteur_beat_t, batteur_beat_deleter>;

int main(int argc, char** argv)
{
    batteur_beat_u beat { batteur_load_beat("") };
    return 0;
}