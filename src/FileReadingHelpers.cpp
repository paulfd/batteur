#include "FileReadingHelpers.h"
#include "MathHelpers.h"
#include "MidiHelpers.h"

tl::optional<unsigned> getQuarterPerBars(const fmidi_event_t& evt)
{
    if (evt.data[0] != 0x58)
        return {};

    return evt.data[1] / (1 << (evt.data[2] - 2));
}

tl::optional<double> getSecondsPerQuarter(const fmidi_event_t& evt)
{
    if (evt.data[0] != 0x51 || evt.datalen != 4)
        return {};

    const uint8_t* d24 = &evt.data[1];
    const uint32_t tempo = (d24[0] << 16) | (d24[1] << 8) | d24[2];
    return 1e-6 * tempo;
}

// Note that the passed file should not be const
tl::expected<batteur::Sequence, MidiFileError> readMidiFile(nlohmann::json& json, const fs::path& rootDirectory)
{
    if (json.is_null())
        return tl::make_unexpected(MidiFileError::NotPresent);

    if (!json.contains("filename"))
        return tl::make_unexpected(MidiFileError::NoFilename);

    fs::path filepath = rootDirectory / json["filename"].get<std::string>();

    fmidi_smf_u midiFile { fmidi_smf_file_read(filepath.c_str()) };
    if (!midiFile) {
        DBG("Cannot read file {} from root directory {}", json["filename"].get<std::string>(), rootDirectory.native());
        return {};
    }
    batteur::Sequence returned;

    const auto ib = json["ignore_bars"];
    if (!ib.is_null() && !ib.is_number_unsigned())
        return tl::make_unexpected(MidiFileError::WrongIgnoreBars);
    const auto ignoreBars = ib.is_null() ? 0 : ib.get<unsigned>();

    const auto b = json["bars"];
    if (!b.is_null() && !b.is_number_unsigned())
        return tl::make_unexpected(MidiFileError::WrongBars);
    const auto bars = b.is_null() ? 2 : b.get<unsigned>();
    DBG("Bars to get {}", bars);

    const auto findLastNoteOn = [&returned](uint8_t number, double time) -> void {
        for (auto it = returned.rbegin(); it != returned.rend(); ++it) {
            if (it->number == number) {
                it->duration = max(0.0, time - it->timestamp);
                return;
            }
        }
    };

    fmidi_seq_u midiSequencer { fmidi_seq_new(midiFile.get()) };
    fmidi_seq_event_t event;
    unsigned quarterPerBars { 4 };
    double secondsPerQuarter { 0.5 };
    auto ignoredQuarters = static_cast<double>(quarterPerBars * ignoreBars);
    auto fileEnd = static_cast<double>(quarterPerBars * (ignoreBars + bars));
    while (fmidi_seq_next_event(midiSequencer.get(), &event)) {
        const auto& evt = event.event;
        switch (evt->type) {
        case (fmidi_event_meta):
            if (auto qpb = getQuarterPerBars(*evt)) {
                quarterPerBars = *qpb;
                ignoredQuarters = static_cast<double>(quarterPerBars * ignoreBars);
                fileEnd = static_cast<double>(quarterPerBars * (ignoreBars + bars));
            } else if (auto spq = getSecondsPerQuarter(*evt)) {
                secondsPerQuarter = *spq;
            }
            break;
        case (fmidi_event_message):
            // go to the next process step
            break;
        default:
            // Ignore other messages
            continue;
        }

        const double timeInQuarters = event.time / secondsPerQuarter;
        
        if (timeInQuarters < ignoredQuarters)
            continue;

        if (timeInQuarters > fileEnd)
            break;

        switch (midi::status(evt->data[0])) {
        case midi::noteOff:
            findLastNoteOn(evt->data[1], timeInQuarters);
            break;
        case midi::noteOn:
            // It's a note-off
            if (evt->data[2] == 0) {
                findLastNoteOn(evt->data[1], timeInQuarters);
                break;
            }

            // It's a real note-on
            returned.push_back({ timeInQuarters, 0.0, evt->data[1], evt->data[2] });
            break;
        default:
            break;
        }
    }

    if (returned.empty())
        return tl::make_unexpected(MidiFileError::NoDataRead);

    for (auto& note : returned) {
        note.timestamp -= ignoredQuarters;
    }

#if 0
    DBG("Note NUM: TIME (DURATION)");
    for (auto& note : returned) {
        DBG("Note {} : {:.2f} ({:.2f})", +note.number, note.timestamp, note.duration );
    }
#endif
    return returned;
}

tl::expected<double, BPMError> checkBPM(const nlohmann::json& bpm)
{
    if (bpm.is_null())
        return tl::make_unexpected(BPMError::NotPresent);

    if (!bpm.is_number())
        return tl::make_unexpected(BPMError::NotANumber);

    const auto b = bpm.get<double>();

    if (b <= 0.0)
        return tl::make_unexpected(BPMError::Negative);

    return b;
}

tl::expected<unsigned, QuarterPerBarsError> checkQuartersPerBar(const nlohmann::json& qpb)
{
    if (qpb.is_null())
        return tl::make_unexpected(QuarterPerBarsError::NotPresent);

    if (!qpb.is_number_unsigned())
        return tl::make_unexpected(QuarterPerBarsError::NotAnUnsigned);

    const auto q = qpb.get<unsigned>();

    if (q == 0)
        return tl::make_unexpected(QuarterPerBarsError::Zero);

    return q;
}