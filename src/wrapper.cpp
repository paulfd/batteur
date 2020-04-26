#include "batteur.h"
#include "BeatDescription.h"
#include "Player.h"

#ifdef __cplusplus
extern "C" {
#endif

batteur_beat_t* batteur_load_beat(const char* filename)
{
    std::error_code ec;
    auto beat = batteur::BeatDescription::buildFromFile(filename, ec);
    if (ec)
        return NULL;

    return reinterpret_cast<batteur_beat_t*>(beat.release());
}

void batteur_free_beat(batteur_beat_t* beat)
{
    delete reinterpret_cast<batteur::BeatDescription*>(beat);
}

batteur_player_t* batteur_new()
{
    return reinterpret_cast<batteur_player_t*>(new batteur::Player);
}

void batteur_free(batteur_player_t* player)
{
    delete reinterpret_cast<batteur::Player*>(player);
}

bool batteur_load(batteur_player_t* player, batteur_beat_t* beat)
{
    if (!player || !beat)
        return false;

    auto self = reinterpret_cast<batteur::Player*>(player);
    auto description = reinterpret_cast<batteur::BeatDescription*>(beat);    
    return self->loadBeatDescription(*description);
}

void batteur_set_sample_rate(batteur_player_t* player, double sample_rate)
{
    if (!player)
        return;
    
    auto self = reinterpret_cast<batteur::Player*>(player);
    self->setSampleRate(sample_rate);
}

void batteur_note_cb(batteur_player_t* player, batteur_note_cb_t callback, void* cbdata)
{
    if (!player)
        return;
    
    auto self = reinterpret_cast<batteur::Player*>(player);
    self->setNoteCallback([=](int delay, uint8_t number, uint8_t velocity) {
        callback(delay, number, velocity, cbdata);
    });
}

void batteur_set_tempo(batteur_player_t* player, double bpm)
{
    if (!player)
        return;
    
    auto self = reinterpret_cast<batteur::Player*>(player);
    self->setTempo(bpm);
}

void batteur_tick(batteur_player_t* player, int sample_count)
{
    if (!player)
        return;
    
    auto self = reinterpret_cast<batteur::Player*>(player);
    self->tick(sample_count);
}

void batteur_fill_in(batteur_player_t* player)
{
    if (!player)
        return;
    
    auto self = reinterpret_cast<batteur::Player*>(player);
    self->fillIn();
}

void batteur_next(batteur_player_t* player)
{
    if (!player)
        return;
    
    auto self = reinterpret_cast<batteur::Player*>(player);
    assert(false);
}

void batteur_stop(batteur_player_t* player)
{
    if (!player)
        return;
    
    auto self = reinterpret_cast<batteur::Player*>(player);
    self->stop();
}

void batteur_pause(batteur_player_t* player)
{
    if (!player)
        return;
    
    auto self = reinterpret_cast<batteur::Player*>(player);
    assert(false);
}


#ifdef __cplusplus
}
#endif
