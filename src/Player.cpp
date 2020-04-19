#include "Player.h"
#include "BeatDescription.h"

namespace batteur {

Player::Player()
{
    queuedSequences.reserve(4);
}

bool Player::loadBeatDescription(const BeatDescription& description)
{
    if (description.parts.empty())
        return false;

    currentBeat = &description;
    position = 0.0;
    queuedSequences.clear();
    queuedSequences.push_back({ &currentBeat->parts[0].mainLoop, false });
    return true;
}

void Player::start()
{

}

void Player::stop()
{

}

void Player::fillIn()
{

}

void Player::tick(int sampleCount)
{
    if (!currentBeat)
        return;

    if (queuedSequences.empty())
        return;

    double blockEnd = position + sampleCount / sampleRate / secondsPerQuarter;
    auto current = queuedSequences.begin();
    auto noteIt = current->sequence->begin();

    while (true) {
        noteIt = std::find_if(
            noteIt,
            current->sequence->end(),
            [&](const Sequence::value_type& v) { return v.timestamp >= position; });

        if (noteIt == current->sequence->end()) {
            const auto sequenceDuration = totalDuration(*current->sequence);
            if (blockEnd < sequenceDuration) {
                position = blockEnd;
                break;
            }

            blockEnd = sequenceDuration - position;
            position = 0.0;

            if (queuedSequences.size() > 1) {
                current = queuedSequences.erase(current);
            }

            noteIt = current->sequence->begin();
            continue;
        }

        if (noteIt->timestamp > blockEnd) {
            position = blockEnd;
            break;
        }

        const int noteOnDelay = static_cast<int>((noteIt->timestamp - position) * secondsPerQuarter * sampleRate);
        const int noteOffDelay = static_cast<int>((noteIt->timestamp + noteIt->duration - position) * secondsPerQuarter * sampleRate);
        deferredNotes.push_back({ noteOnDelay, noteIt->number, noteIt->velocity });
        deferredNotes.push_back({ noteOffDelay, noteIt->number, 0 });
        position = noteIt->timestamp;
        noteIt++;
    }

    std::sort(deferredNotes.begin(), deferredNotes.end(), [](const NoteEvents& lhs, const NoteEvents& rhs) {
        return lhs.delay < rhs.delay;
    });

    auto deferredIt = deferredNotes.begin();
    while (deferredIt != deferredNotes.end() && deferredIt->delay < sampleCount) {
        noteCallback(deferredIt->delay, deferredIt->number, deferredIt->velocity);
        deferredIt++;
    }

    deferredNotes.erase(deferredNotes.begin(), deferredIt);

    for (auto& evt : deferredNotes)
        evt.delay -= sampleCount;
}

void Player::setSampleRate(double sampleRate)
{
    this->sampleRate = sampleRate;
}

void Player::setTempo(double bpm)
{
    this->secondsPerQuarter = 60.0 / bpm;
}

void Player::setNoteCallback(NoteCallback cb)
{
    noteCallback = std::move(cb);
}

const char* Player::getCurrentPartName()
{
    return {};
}

double Player::totalDuration(const Sequence& sequence)
{
    if (sequence.empty())
        return 0.0;

    return std::ceil(sequence.back().timestamp);
}

}