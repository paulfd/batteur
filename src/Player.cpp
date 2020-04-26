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
    return true;
}

bool Player::start()
{        
    return messages.try_push(Message::Start);
}

bool Player::stop()
{
    return messages.try_push(Message::Stop);
}

bool Player::fillIn()
{
    return messages.try_push(Message::Fill);
}

bool Player::next()
{
    return messages.try_push(Message::Next);
}

void Player::_start()
{
    if (currentBeat->intro) {
        queuedSequences.push_back(&*currentBeat->intro);
        queuedSequences.push_back(&currentBeat->parts[partIndex].mainLoop);
        state = State::Intro;
    } else {
        queuedSequences.push_back(&currentBeat->parts[partIndex].mainLoop);
        state = State::Playing;
    }
}

void Player::_stop()
{
    while (queuedSequences.size() > 1)
        queuedSequences.pop_back();
    
    if (currentBeat->ending)
        queuedSequences.push_back(&*currentBeat->ending);

    state = State::Ending;
}

void Player::_fillIn()
{
    if (currentBeat->parts[partIndex].fills.empty())
        return;
    
    queuedSequences.push_back(&currentBeat->parts[partIndex].fills[fillIndex]);
    queuedSequences.push_back(&currentBeat->parts[partIndex].mainLoop);
    state = State::Fill;
    fillIndex = (fillIndex + 1) % currentBeat->parts[partIndex].fills.size();
}

void Player::_next()
{
    switch (queuedSequences.size()) {
    case 3:
        if (currentBeat->parts[partIndex].transition) {
            queuedSequences.pop_back();
            queuedSequences.pop_back();
            DBG("Adding a transition");
            queuedSequences.push_back(&*currentBeat->parts[partIndex].transition);
        }
        break;
    case 2:
        queuedSequences.pop_back();
        break;
    case 1:
        if (currentBeat->parts[partIndex].transition) {
            DBG("Adding a transition");
            queuedSequences.push_back(&*currentBeat->parts[partIndex].transition);
        }
        break;
    default:
        break;
    }
    partIndex = (partIndex + 1) % currentBeat->parts.size();
    fillIndex = 0;
    queuedSequences.push_back(&currentBeat->parts[partIndex].mainLoop);
    state = State::Next;   
}

void Player::updateState()
{
    Message msg;
    while (messages.try_pop(msg)) {
        switch(msg) {
        case Message::Start:
            if (state == State::Stopped)
                _start();
            break;
        case Message::Stop:
            if (state != State::Stopped && state != State::Ending)
                _stop();
            break;
        case Message::Fill:
            if (state == State::Playing)
                _fillIn();
            break;
        case Message::Next:
            if (state == State::Playing || state == State::Fill)
                _next();
            break;
        }
    }
}


void Player::tick(int sampleCount)
{
    if (!currentBeat)
        return;

    updateState();

    if (queuedSequences.empty())
        return;

    double blockEnd = position + sampleCount / sampleRate / secondsPerQuarter;
    auto current = queuedSequences.front(); // Otherwise we have ** everywhere..
    auto noteIt = current->begin();

    while (state != State::Stopped) {
        noteIt = std::find_if(
            noteIt,
            queuedSequences.front()->end(),
            [&](const Sequence::value_type& v) { return v.timestamp >= position; });

        if (noteIt == current->end()) {
            const auto sequenceDuration = totalDuration(*queuedSequences.front());
            if (blockEnd < sequenceDuration) {
                position = blockEnd;
                break;
            }

            blockEnd = sequenceDuration - position;
            position = 0.0;

            if (queuedSequences.size() > 1) {
                DBG("Removing the top sequence");
                queuedSequences.erase(queuedSequences.begin());
                noteIt = queuedSequences.front()->begin();
            } else if (state == State::Ending) {
                queuedSequences.clear();
                state = State::Stopped;
                break;
            } 

            if (queuedSequences.size() == 1)
                state = State::Playing;
        }

        if (queuedSequences.size() == 3) {
            // Fill in or transition state
            if (noteIt->timestamp > queuedSequences[1]->front().timestamp) {
                DBG("Fill-in state: removing the top sequence");
                queuedSequences.erase(queuedSequences.begin());
                noteIt = queuedSequences.front()->begin();
                continue;
            }
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

bool Player::isPlaying() const
{
    return state != State::Stopped;
}

}