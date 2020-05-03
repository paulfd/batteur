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

    state = State::Stopped;
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
    const auto& currentTransition = currentBeat->parts[partIndex].transition;
    if (enteringFillInState()) {
        queuedSequences.pop_back(); // Remove the back (which should be the next part)
        if (currentTransition) {
            DBG("Adding a transition");
            queuedSequences.pop_back();
            queuedSequences.push_back(&currentTransition.value());
        }
    } else if (leavingFillInState()) {
        queuedSequences.pop_back(); // Remove the back (which should be the next part)
    } else {
        if (currentTransition) {
            DBG("Adding a transition");
            queuedSequences.push_back(&currentTransition.value());
        }
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
        switch (msg) {
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

    const auto currentQPB = currentBeat->quartersPerBar;
    auto blockStart = position;
    double blockEnd = blockStart + sampleCount / sampleRate / secondsPerQuarter;
    const auto midiDelay = [&] (double timestamp) -> int {
        return static_cast<int>(
            (timestamp - blockStart) * secondsPerQuarter * sampleRate
        );
    };

    auto current = queuedSequences.front(); // Otherwise we have ** everywhere..
    auto noteIt = current->begin();

    const auto barStartedAt = [currentQPB] (double pos) -> double {
        const auto qpb = static_cast<double>(currentQPB);
        const auto barPosition = pos / qpb;
        return std::floor(barPosition) * qpb;
    };

    const auto movePosition = [&] (double offset) {
        blockEnd += offset; 
        blockStart += offset; 
        position += offset;
    };

    const auto eraseFrontSequence = [&] {
        queuedSequences.erase(queuedSequences.begin());
        current = queuedSequences.front();
        noteIt = current->begin();
        movePosition(-barStartedAt(position));
    };


    while (state != State::Stopped) {
        noteIt = std::find_if(
            noteIt,
            current->end(),
            [&](const Sequence::value_type& v) { return v.timestamp >= position; }
        );
    
        if (noteIt == current->end()) {
            if (leavingFillInState()) {
                // Exiting the fill-in state
                // DBG("Exiting fill-in state: removing the top sequence");
                eraseFrontSequence();
                if (state != State::Ending)
                    state = State::Playing;
                continue;
            }

            const auto sequenceDuration = 
                barCount(*current, currentQPB) * currentQPB;

            if (blockEnd < sequenceDuration) {
                position = blockEnd;
                break;
            }

            blockEnd -= sequenceDuration;
            position = 0.0;

            if (state == State::Ending) {
                queuedSequences.clear();
                state = State::Stopped;
                break;
            }
        }        

        if (enteringFillInState()) {
            const auto barStart = barStartedAt(position);
            const auto relPosition = position - barStart;
            const auto barThreshold = static_cast<double>(currentBeat->quartersPerBar) - 1;
            const auto relFillStart = queuedSequences[1]->front().timestamp;
            // DBG("Could start fill in; relative position {:.2f}, fill start at {:.2f}", relPosition, relFillStart);
            if (relPosition > barThreshold) {
                if(relFillStart > barThreshold) {
                    // DBG("Fill-in has a short bar; starting fill-in now");
                    eraseFrontSequence();
                    continue;
                }
                // DBG("Deferring fill in next bar");
            } else {
                // DBG("Starting fill-in now", position);
                eraseFrontSequence();
                if (relFillStart > barThreshold) {
                    // DBG("Fill-in has a short bar; skipping the first fill bar");
                    movePosition(static_cast<double>(currentBeat->quartersPerBar));
                } 
                continue;
            }
        }

        if (noteIt->timestamp > blockEnd) {
            position = blockEnd;
            break;
        }

#if 0
        DBG("Seq: {} | Pos/BlockEnd: {:2.2f}/{:2.2f} | current note (index/time/duration) : {}/{:.2f}/{:.2f}",
            queuedSequences.size(), 
            position,
            blockEnd,
            std::distance(queuedSequences.front()->begin(), noteIt),
            noteIt->timestamp,
            noteIt->duration);
#endif

        const int noteOnDelay = midiDelay(noteIt->timestamp);
        const int noteOffDelay = midiDelay(noteIt->timestamp + noteIt->duration);
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

bool Player::enteringFillInState() const
{
    return queuedSequences.size() == 3;
}

bool Player::leavingFillInState() const
{
    return queuedSequences.size() == 2;
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