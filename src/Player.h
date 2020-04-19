#pragma once
#include "BeatDescription.h"
namespace batteur {

using NoteCallback = std::function<void(int, uint8_t, uint8_t)>;

class Player {
public:
    Player();
    bool loadBeatDescription(const BeatDescription& description);
    void start();
    void stop();
    void fillIn();
    void tick(int sampleCount);

    void setSampleRate(double sampleRate);
    void setTempo(double bpm);
    void setNoteCallback(NoteCallback cb);
    const char* getCurrentPartName();

private:
    struct NoteEvents {
        int delay;
        uint8_t number;
        uint8_t velocity;
    };
    struct QueuedSequence {
        const Sequence* sequence;
        bool fill;
    };
    static double totalDuration(const Sequence& sequence);
    const BeatDescription* currentBeat { nullptr };
    double position { 0.0 };
    std::vector<QueuedSequence> queuedSequences;
    std::vector<NoteEvents> deferredNotes;
    NoteCallback noteCallback {};
    double secondsPerQuarter { 0.5 };
    double sampleRate { 48e3 };
    int fillIndex { 0 };
};

}