# Batteur library and LV2 plugin

This repository holds the `batteur` library, a backing drum machine that can be controlled using a single switch.
It contains both a shared library version and an LV2 plugin.
It is meant to be used with a drum sound generator within a more global LV2 or Jack session.

Yet to do:
- Update tests and harden the thing
- Refine the handling of the LV2 time extension for synchronization with the host tempo and time grid
- UI, although these are in other projects

## Beat description

The format is not really final as I'm still playing around with it.
This is how it should look for now:

```json
{
    "name": "Surf Beat 1",
    "group": "Pop",
    "bpm" : 160,
    "quarters_per_bar": 4,
    "ending": { "filename" : "Fills/160 Surf 42TF Fill 2.mid" },
    "parts": [
        {
            "name": "Hats",
            "midi_file" : { "filename" : "Grooves/160 Surf 42TF Hat.mid", "ignore_bars" : 1, "bars": 4 },
            "fills": [
                { "filename" : "Hat Fills/160 Surf 42TF Hat Fill 1.mid" },
                { "filename" : "Hat Fills/160 Surf 42TF Hat Fill 2.mid" },
                { "filename" : "Hat Fills/160 Surf 42TF Hat Fill 3.mid" },
                { "filename" : "Hat Fills/160 Surf 42TF Hat Fill 4.mid" },
                { "filename" : "Hat Fills/160 Surf 42TF Hat Fill 5.mid" }
            ],
            "transition": { "filename" : "Hat Fills/160 Surf 42TF Hat Fill 6.mid" }
        },
        {
            "name": "Ride",
            "midi_file" : { "filename" : "Grooves/160 Surf 42TF Ride.mid", "ignore_bars" : 1 , "bars": 4},
            "fills": [
                { "filename" : "Ride Fills/160 Surf 42TF Ride Fill 1.mid" },
                { "filename" : "Ride Fills/160 Surf 42TF Ride Fill 2.mid" },
                { "filename" : "Ride Fills/160 Surf 42TF Ride Fill 3.mid" },
                { "filename" : "Ride Fills/160 Surf 42TF Ride Fill 4.mid" },
                { "filename" : "Ride Fills/160 Surf 42TF Ride Fill 4.mid" }
            ],
            "transition": { "filename" : "Ride Fills/160 Surf 42TF Ride Fill 5.mid" }
        }
    ]
}
```

A short press on the main switch triggers a fill, a longer press moves to the next part.
The fill duration tries to be a bit smart: if you press at the end of the bar, it will do a longer fill over the next bar.
It also tries to adapt to different fill durations, although this could be improved.
Double pressing the main switch will trigger the ending.