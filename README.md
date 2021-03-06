# Batteur library and LV2 plugin

This repository holds the `batteur` library, a backing drum machine that can be controlled using a single switch.
It contains both a shared library version and an LV2 plugin.
It is meant to be used with a drum sound generator within a more global LV2 or Jack session.

Yet to do:
- C API documentation
- Update tests and harden the thing a little bit.

## Beat descriptions

The format is not really final, expect it to maybe evolve until the first "real" release.
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
            "sequence" : { "filename" : "Grooves/160 Surf 42TF Hat.mid", "ignore_bars" : 1, "bars": 4 },
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
            "sequence" : { "filename" : "Grooves/160 Surf 42TF Ride.mid", "ignore_bars" : 1 , "bars": 4},
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

You can also use a description within the JSON file.
Check out the included beats in the `beats` directory for this.
There is a development tool that serialize a JSON with midi files to a *monolithic* JSON file in `tools/serialize`.

## LV2 plugin behavior

The LV2 plugin works as follows.
A short press on the main switch triggers a fill, a longer press moves to the next part.
The fill duration tries to be a bit smart: if you press at the end of the bar, it will do a longer fill over the next bar.
It also tries to adapt to different fill durations, although this could be improved.
Double pressing the main switch will trigger the ending, which acts as a fill.

## Compilation

You need to have `cmake` installed, as well as a reasonable compiler.
The cmake options are as follows:

```
BATTEUR_LV2     "Enable LV2 plug-in build [default: ON]"
BATTEUR_TESTS   "Enable tests build [default: OFF]"
BATTEUR_TOOLS   "Enable tools build [default: OFF]"
BATTEUR_SHARED  "Enable the shared library build [default: ON]
BATTEUR_STATIC  "Enable the static library build [default: ON]
```

Enabling the development tools requires the `fmt` library.



