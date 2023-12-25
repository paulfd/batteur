/*
  Batteur LV2 plugin

  Copyright 2020, Paul Ferrand <paul@ferrand.cc>

  This file was based on skeleton and example code from the LV2 plugin 
  distribution available at http://lv2plug.in/

  The LV2 sample plugins have the following copyright and notice, which are 
  extended to the current work:
  Copyright 2011-2016 David Robillard <d@drobilla.net>
  Copyright 2011 Gabriel M. Beddingfield <gabriel@teuton.org>
  Copyright 2011 James Morris <jwm.art.net@gmail.com>

  Permission to use, copy, modify, and/or distribute this software for any
  purpose with or without fee is hereby granted, provided that the above
  copyright notice and this permission notice appear in all copies.

  THIS SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
  WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
  MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
  ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
  ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
  OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#include "lv2/atom/forge.h"
#include "lv2/atom/util.h"
#include "lv2/buf-size/buf-size.h"
#include "lv2/core/lv2.h"
#include "lv2/core/lv2_util.h"
#include "lv2/midi/midi.h"
#include "lv2/options/options.h"
#include "lv2/parameters/parameters.h"
#include "lv2/patch/patch.h"
#include "lv2/state/state.h"
#include "lv2/urid/urid.h"
#include "lv2/worker/worker.h"
#include "lv2/time/time.h"
#include "lv2/log/logger.h"
#include "lv2/log/log.h"

#include <math.h>
#include "batteur.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#define stomper_URI "https://github.com/paulfd/stomper"
#define stomper__beatDescription "https://github.com/paulfd/stomper:beatDescription"
#define stomper__beatName "https://github.com/paulfd/stomper:beatName"
#define CHANNEL_MASK 0x0F
#define NOTE_ON 0x90
#define NOTE_OFF 0x80
#define MIDI_CHANNEL(byte) (byte & CHANNEL_MASK)
#define MIDI_STATUS(byte) (byte & ~CHANNEL_MASK)
#define MAX_BLOCK_SIZE 8192
#define MAX_PATH_SIZE 1024
#define UNUSED(x) (void)(x)

typedef struct
{
    // Features
    LV2_URID_Map* map;
    LV2_URID_Unmap* unmap;
    LV2_Worker_Schedule* worker;
    LV2_Log_Log* log;

    // Ports
    const LV2_Atom_Sequence* input_p;
    LV2_Atom_Sequence* output_p;
    const float* main_p;
    float* time_num_p;
    float* time_denom_p;
    float* bar_position_p;
    float* tempo_p;

    // Atom forge
    LV2_Atom_Forge forge; ///< Forge for writing atoms in run thread
    LV2_Atom_Forge_Frame notify_frame; ///< Cached for worker replies

    // Logger
    LV2_Log_Logger logger;

    // URIs
    LV2_URID midi_event_uri;
    LV2_URID options_interface_uri;
    LV2_URID max_block_length_uri;
    LV2_URID nominal_block_length_uri;
    LV2_URID sample_rate_uri;
    LV2_URID atom_object_uri;
    LV2_URID atom_float_uri;
    LV2_URID atom_int_uri;
    LV2_URID atom_urid_uri;
    LV2_URID atom_string_uri;
    LV2_URID atom_bool_uri;
    LV2_URID atom_path_uri;
    LV2_URID patch_set_uri;
    LV2_URID patch_get_uri;
    LV2_URID patch_put_uri;
    LV2_URID patch_property_uri;
    LV2_URID patch_value_uri;
    LV2_URID patch_body_uri;
    LV2_URID beat_description_uri;
    LV2_URID beat_name_uri;

    batteur_beat_t* currentBeat;
    batteur_beat_t* nextBeat;
    batteur_player_t* player;
    bool expect_nominal_block_length;
    float main_switch_status;
    char beat_file_path[MAX_PATH_SIZE + 1];
    char* bundle_path;
    int max_block_size;
    double sample_rate;
    double bpm;
    int64_t samples_since_last_switch;
    int initial_countdown;
    bool can_go_to_next_bar;
} batteur_plugin_t;

enum {
    INPUT_PORT = 0,
    OUTPUT_PORT,
    MAIN_PORT,
    TIME_NUM_PORT,
    TIME_DENOM_PORT,
    BARPOS_PORT,
    TEMPO_PORT,
};

static void
batteur_map_required_uris(batteur_plugin_t* self)
{
    LV2_URID_Map* map = self->map;
    self->midi_event_uri = map->map(map->handle, LV2_MIDI__MidiEvent);
    self->max_block_length_uri = map->map(map->handle, LV2_BUF_SIZE__maxBlockLength);
    self->nominal_block_length_uri = map->map(map->handle, LV2_BUF_SIZE__nominalBlockLength);
    self->sample_rate_uri = map->map(map->handle, LV2_PARAMETERS__sampleRate);
    self->atom_float_uri = map->map(map->handle, LV2_ATOM__Float);
    self->atom_int_uri = map->map(map->handle, LV2_ATOM__Int);
    self->atom_path_uri = map->map(map->handle, LV2_ATOM__Path);
    self->atom_bool_uri = map->map(map->handle, LV2_ATOM__Bool);
    self->atom_string_uri = map->map(map->handle, LV2_ATOM__String);
    self->atom_urid_uri = map->map(map->handle, LV2_ATOM__URID);
    self->atom_object_uri = map->map(map->handle, LV2_ATOM__Object);
    self->patch_set_uri = map->map(map->handle, LV2_PATCH__Set);
    self->patch_get_uri = map->map(map->handle, LV2_PATCH__Get);
    self->patch_put_uri = map->map(map->handle, LV2_PATCH__Put);
    self->patch_body_uri = map->map(map->handle, LV2_PATCH__body);
    self->patch_property_uri = map->map(map->handle, LV2_PATCH__property);
    self->patch_value_uri = map->map(map->handle, LV2_PATCH__value);
    self->beat_description_uri = map->map(map->handle, stomper__beatDescription);
    self->beat_name_uri = map->map(map->handle, stomper__beatName);
}

static void
connect_port(LV2_Handle instance,
    uint32_t port,
    void* data)
{
    batteur_plugin_t* self = (batteur_plugin_t*)instance;
    // lv2_log_note(&self->logger, "[connect_port] Called for index %d on address %p\n", port, data);
    switch (port) {
    case INPUT_PORT:
        self->input_p = (const LV2_Atom_Sequence*)data;
        break;
    case OUTPUT_PORT:
        self->output_p = (LV2_Atom_Sequence*)data;
        break;
    case MAIN_PORT:
        self->main_p = (const float*)data;
        break;
    case TIME_NUM_PORT:
        self->time_num_p = (float*)data;
        break;
    case TIME_DENOM_PORT:
        self->time_denom_p = (float*)data;
        break;
    case BARPOS_PORT:
        self->bar_position_p = (float*)data;
        break;
    case TEMPO_PORT:
        self->tempo_p = (float*)data;
        break;
    default:
        break;
    }
}

static void
batteur_callback(int delay, uint8_t number, float value, void* cbdata)
{
    batteur_plugin_t* self = (batteur_plugin_t*)cbdata;
    
    uint8_t msg[3] = { 
        value > 0 ? NOTE_ON : NOTE_OFF,
        number,
        (uint8_t)(value * 127.0f)
    };

    LV2_Atom atom = { 
        .type = self->midi_event_uri,
        .size = sizeof(msg)
    };

    if (delay < 0) {
        lv2_log_error(&self->logger, "Negative delay in callback: %d\n", delay);
        delay = 0;
    }

	if (!lv2_atom_forge_frame_time(&self->forge, delay))
        return;

	if (!lv2_atom_forge_raw(&self->forge, &atom, sizeof(LV2_Atom)))
        return;

	if (!lv2_atom_forge_raw(&self->forge, msg, sizeof(msg)))
        return;

	lv2_atom_forge_pad(&self->forge, sizeof(LV2_Atom) + sizeof(msg));
}

static LV2_Handle
instantiate(const LV2_Descriptor* descriptor,
    double rate,
    const char* path,
    const LV2_Feature* const* features)
{
    UNUSED(descriptor);
    UNUSED(path);
    LV2_Options_Option* options = NULL;
    bool supports_bounded_block_size = false;
    bool options_has_block_size = false;
    bool supports_fixed_block_size = false;

    // Allocate and initialise instance structure.
    batteur_plugin_t* self = (batteur_plugin_t*)calloc(1, sizeof(batteur_plugin_t));
    if (!self)
        return NULL;

    // Set defaults
    self->max_block_size = MAX_BLOCK_SIZE;
    self->sample_rate = rate;
    self->expect_nominal_block_length = false;
    self->beat_file_path[0] = '\0';
    self->beat_file_path[MAX_PATH_SIZE] = '\0';
    self->main_switch_status = 0.0f;
    self->nextBeat = NULL;
    self->bundle_path = malloc(strlen(path) + 1);
    strcpy(self->bundle_path, path);

    // Get the features from the host and populate the structure
    for (const LV2_Feature* const* f = features; *f; f++) {
        // lv2_log_note(&self->logger, "Feature URI: %s\n", (**f).URI);

        if (!strcmp((**f).URI, LV2_URID__map))
            self->map = (**f).data;

        if (!strcmp((**f).URI, LV2_URID__unmap))
            self->unmap = (**f).data;

        if (!strcmp((**f).URI, LV2_BUF_SIZE__boundedBlockLength))
            supports_bounded_block_size = true;

        if (!strcmp((**f).URI, LV2_BUF_SIZE__fixedBlockLength))
            supports_fixed_block_size = true;

        if (!strcmp((**f).URI, LV2_OPTIONS__options))
            options = (**f).data;

        if (!strcmp((**f).URI, LV2_WORKER__schedule))
            self->worker = (**f).data;

        if (!strcmp((**f).URI, LV2_LOG__log))
            self->log = (**f).data;
    }

    // Setup the loggers
    lv2_log_logger_init(&self->logger, self->map, self->log);

    // The map feature is required
    if (!self->map) {
        lv2_log_error(&self->logger, "Map feature not found, aborting...\n");
        goto abort;
    }

    // The worker feature is required
    if (!self->worker) {
        lv2_log_error(&self->logger, "Worker feature not found, aborting...\n");
        goto abort;
    }

    // Map the URIs we will need
    batteur_map_required_uris(self);

    // Initialize the forge
    lv2_atom_forge_init(&self->forge, self->map);

    // Check the options for the block size and sample rate parameters
    if (options) {
        for (const LV2_Options_Option* opt = options; opt->key || opt->value; ++opt) {
            if (opt->key == self->sample_rate_uri) {
                if (opt->type != self->atom_float_uri) {
                    lv2_log_warning(&self->logger, "Got a sample rate but the type was wrong\n");
                    continue;
                }
                self->sample_rate = *(float*)opt->value;
            } else if (!self->expect_nominal_block_length && opt->key == self->max_block_length_uri) {
                if (opt->type != self->atom_int_uri) {
                    lv2_log_warning(&self->logger, "Got a max block size but the type was wrong\n");
                    continue;
                }
                self->max_block_size = *(int*)opt->value;
                options_has_block_size = true;
            } else if (opt->key == self->nominal_block_length_uri) {
                if (opt->type != self->atom_int_uri) {
                    lv2_log_warning(&self->logger, "Got a nominal block size but the type was wrong\n");
                    continue;
                }
                self->max_block_size = *(int*)opt->value;
                self->expect_nominal_block_length = true;
                options_has_block_size = true;
            }
        }
    } else {
        lv2_log_warning(&self->logger,
            "No option array was given upon instantiation; will use default values\n.");
    }

    // We need _some_ information on the block size
    if (!supports_bounded_block_size && !supports_fixed_block_size && !options_has_block_size) {
        lv2_log_error(&self->logger,
            "Bounded block size not supported and options gave no block size, aborting...\n");
        goto abort;
    }

    self->player = batteur_new();
    batteur_note_cb(self->player, &batteur_callback, (void*)self);
    self->samples_since_last_switch = 0;
    self->can_go_to_next_bar = false;
    self->initial_countdown = 4;
    self->bpm = 120.0f;
    return (LV2_Handle)self;

abort:
    free(self->bundle_path);
    free(self);
    return NULL;
}

static void
cleanup(LV2_Handle instance)
{
    batteur_plugin_t* self = (batteur_plugin_t*)instance;
    batteur_free_beat(self->currentBeat);
    batteur_free_beat(self->nextBeat);
    batteur_free(self->player);
    free(self->bundle_path);
    free(self);
}

static void
activate(LV2_Handle instance)
{
    UNUSED(instance);
}

static void
deactivate(LV2_Handle instance)
{
    UNUSED(instance);
}

static void 
send_file_path(batteur_plugin_t* self)
{
    LV2_Atom_Forge_Frame frame;
    lv2_atom_forge_frame_time(&self->forge, 0);
    lv2_atom_forge_object(&self->forge, &frame, 0, self->patch_set_uri);
    lv2_atom_forge_key(&self->forge, self->patch_property_uri);
    lv2_atom_forge_urid(&self->forge, self->beat_description_uri);
    lv2_atom_forge_key(&self->forge, self->patch_value_uri);
    lv2_atom_forge_path(&self->forge, self->beat_file_path, strlen(self->beat_file_path));
    lv2_atom_forge_pop(&self->forge, &frame);
}

static void 
send_beat_name(batteur_plugin_t* self)
{
    LV2_Atom_Forge_Frame frame;
    lv2_atom_forge_frame_time(&self->forge, 0);
    lv2_atom_forge_object(&self->forge, &frame, 0, self->patch_set_uri);
    lv2_atom_forge_key(&self->forge, self->patch_property_uri);
    lv2_atom_forge_urid(&self->forge, self->beat_name_uri);
    lv2_atom_forge_key(&self->forge, self->patch_value_uri);

    const char* name = batteur_get_beat_name(self->currentBeat);
    if (name)
        lv2_atom_forge_string(&self->forge, name, strlen(name));
    else
        lv2_atom_forge_string(&self->forge, "", 0);

    lv2_atom_forge_pop(&self->forge, &frame);
}

static void
main_switch_event(batteur_plugin_t* self, float switch_status)
{
    if (switch_status != 1.0f)
        return;

    if (self->samples_since_last_switch == 0)
        return;

    const double quarter_period = (double)self->samples_since_last_switch / self->sample_rate;
    lv2_log_note(&self->logger, "Quarter period: %f\n", quarter_period);
    self->bpm = 60.0 / quarter_period;
    if (self->initial_countdown < 4)
        batteur_set_tempo(self->player, self->bpm);

    self->samples_since_last_switch = 0;

    if (self->initial_countdown > 0) {
        self->initial_countdown -= 1;
        lv2_log_note(&self->logger, "Countdown %d\n", self->initial_countdown);
        

        if (self->initial_countdown == 0) {
            lv2_log_note(&self->logger, "Starting beat\n");
            batteur_start(self->player);
        }
    } else {
        lv2_log_note(&self->logger, "Can go to next bar!\n");
        self->can_go_to_next_bar = true;
    }
}

static void
beat_description_event(batteur_plugin_t* self, const LV2_Atom* atom)
{
    // If the parameter is different from the current one we send it through
    if (self->beat_file_path[0] == 0 || strncmp(self->beat_file_path, LV2_ATOM_BODY_CONST(atom), strlen(self->beat_file_path)))
        self->worker->schedule_work(self->worker->handle, lv2_atom_total_size(atom), atom);
}

static void
handle_patch_set(batteur_plugin_t* self, const LV2_Atom_Object* obj, int64_t frame)
{
    UNUSED(frame);
    const LV2_Atom* property = NULL;
    lv2_atom_object_get(obj, self->patch_property_uri, &property, 0);
    if (!property) {
        lv2_log_error(&self->logger, "Could not get the property from the patch object, aborting.\n");
        return;
    }

    if (property->type != self->atom_urid_uri) {
        lv2_log_error(&self->logger, "Atom type was not a URID, aborting.\n");
        return;
    }

    const uint32_t key = ((const LV2_Atom_URID*)property)->body;
    const LV2_Atom* atom = NULL;
    lv2_atom_object_get(obj, self->patch_value_uri, &atom, 0);
    if (!atom) {
        lv2_log_error(&self->logger, "[handle_object] Error retrieving the atom, aborting.\n");
        if (self->unmap)
            lv2_log_warning(&self->logger,
                "Atom URI: %s\n",
                self->unmap->unmap(self->unmap->handle, key));
        return;
    }

    if (key == self->beat_description_uri) {
        beat_description_event(self, atom);
    } else {
        lv2_log_warning(&self->logger, "[handle_object] Unknown or unsupported object.\n");
        if (self->unmap)
            lv2_log_warning(&self->logger,
                "Object URI: %s\n",
                self->unmap->unmap(self->unmap->handle, key));
        return;
    }
}

static void
handle_patch_get(batteur_plugin_t* self, const LV2_Atom_Object* obj, int64_t frame)
{
    UNUSED(frame);
    const LV2_Atom_URID* property = NULL;
    lv2_atom_object_get(obj, self->patch_property_uri, &property, 0);
    if (!property) // Send the full state
    {
        send_file_path(self);
        send_beat_name(self);
    } else if (property->body == self->beat_description_uri) {
        send_file_path(self);
    } else if (property->body == self->beat_name_uri) {
        send_beat_name(self);
    }
}

static void
run(LV2_Handle instance, uint32_t sample_count)
{
    batteur_plugin_t* self = (batteur_plugin_t*)instance;
    if (!self->input_p || !self->output_p)
        return;

    // Set up forge to write directly to notify output port.
    const size_t notify_capacity = self->output_p->atom.size;
    lv2_atom_forge_set_buffer(&self->forge, (uint8_t*)self->output_p, notify_capacity);

    // Start a sequence in the notify output port.
    lv2_atom_forge_sequence_head(&self->forge, &self->notify_frame, 0);

    LV2_ATOM_SEQUENCE_FOREACH(self->input_p, ev)
    {
        // If the received atom is an object/patch message
        if (ev->body.type == self->atom_object_uri) {
            const LV2_Atom_Object* obj = (const LV2_Atom_Object*)&ev->body;
            if (obj->body.otype == self->patch_set_uri) {
                handle_patch_set(self, obj, ev->time.frames);
            } else if (obj->body.otype == self->patch_get_uri) {
                handle_patch_get(self, obj, ev->time.frames);
            } else {
                lv2_log_warning(&self->logger, "Got an Object atom but it was not supported.\n");
                if (self->unmap)
                    lv2_log_warning(&self->logger,
                        "Object URI: %s\n",
                        self->unmap->unmap(self->unmap->handle, obj->body.otype));
                continue;
            }
            // Got an atom that is a MIDI event
        } else if (ev->body.type == self->midi_event_uri) {
            // Nothing to do here
        }
    }

    if (*self->main_p != self->main_switch_status) {
        main_switch_event(self, *self->main_p);
        self->main_switch_status = *self->main_p;
        send_beat_name(self);
    }
    self->samples_since_last_switch += sample_count;

    const bool playing = batteur_get_status(self->player) != BATTEUR_STOPPED;
    const double bar_position = batteur_get_bar_position(self->player);
    const double left_in_bar = bar_position - (int)bar_position;
    
    if (playing && !self->can_go_to_next_bar) {
        lv2_log_note(&self->logger, "Waiting for tapping!\n");
        const uint32_t left_in_bar_samples = (uint32_t)(left_in_bar * self->sample_rate);
        sample_count = left_in_bar_samples < sample_count ? left_in_bar_samples : sample_count;
    }

    batteur_tick(self->player, sample_count);

    const double position_after_run = left_in_bar + (double) sample_count / self->sample_rate;
    if (playing && position_after_run > 1.0) {
        lv2_log_note(&self->logger, "Next_bar\n");
        self->can_go_to_next_bar = false;
    }

    *self->time_num_p = batteur_get_time_numerator(self->currentBeat);
    *self->time_denom_p = batteur_get_time_denominator(self->currentBeat);
    *self->bar_position_p = batteur_get_bar_position(self->player);
    *self->tempo_p = self->bpm;
}

static uint32_t
lv2_get_options(LV2_Handle instance, LV2_Options_Option* options)
{
    UNUSED(instance);
    UNUSED(options);
    // We have no options
    return LV2_OPTIONS_ERR_UNKNOWN;
}

static uint32_t
lv2_set_options(LV2_Handle instance, const LV2_Options_Option* options)
{
    batteur_plugin_t* self = (batteur_plugin_t*)instance;

    // Update the block size and sample rate as needed
    for (const LV2_Options_Option* opt = options; opt->value; ++opt) {
        if (opt->key == self->sample_rate_uri) {
            if (opt->type != self->atom_float_uri) {
                lv2_log_warning(&self->logger, "Got a sample rate but the type was wrong\n");
                continue;
            }
            self->sample_rate = *(float*)opt->value;
            batteur_set_sample_rate(self->player, self->sample_rate);
        } else if (!self->expect_nominal_block_length && opt->key == self->max_block_length_uri) {
            if (opt->type != self->atom_int_uri) {
                lv2_log_warning(&self->logger, "Got a max block size but the type was wrong\n");
                continue;
            }
            self->max_block_size = *(int*)opt->value;
            // sfizz_set_samples_per_block(self->synth, self->max_block_size);
        } else if (opt->key == self->nominal_block_length_uri) {
            if (opt->type != self->atom_int_uri) {
                lv2_log_warning(&self->logger, "Got a nominal block size but the type was wrong\n");
                continue;
            }
            self->max_block_size = *(int*)opt->value;
            // sfizz_set_samples_per_block(self->synth, self->max_block_size);
        }
    }
    return LV2_OPTIONS_SUCCESS;
}

static LV2_State_Status
restore(LV2_Handle instance,
    LV2_State_Retrieve_Function retrieve,
    LV2_State_Handle handle,
    uint32_t flags,
    const LV2_Feature* const* features)
{
    UNUSED(flags);
    UNUSED(features);
    batteur_plugin_t* self = (batteur_plugin_t*)instance;

    // Fetch back the saved file path, if any
    size_t size;
    uint32_t type;
    uint32_t val_flags;
    const void* value;
    value = retrieve(handle, self->beat_description_uri, &size, &type, &val_flags);
    if (value && self->player) {
        lv2_log_note(&self->logger, "Restoring the file %s\n", (const char*)value);
        batteur_beat_t* beat = batteur_load_beat((const char *)value);
        if (beat) {
            batteur_stop(self->player);
            strcpy(self->beat_file_path, (const char *)value);
            self->currentBeat = beat;
            batteur_load(self->player, beat);
            self->initial_countdown = 4;
        }
    }
    return LV2_STATE_SUCCESS;
}

static LV2_State_Status
save(LV2_Handle instance,
    LV2_State_Store_Function store,
    LV2_State_Handle handle,
    uint32_t flags,
    const LV2_Feature* const* features)
{
    UNUSED(flags);
    UNUSED(features);
    batteur_plugin_t* self = (batteur_plugin_t*)instance;
    // Save the file path
    store(handle,
        self->beat_description_uri,
        self->beat_file_path,
        strlen(self->beat_file_path) + 1,
        self->atom_path_uri,
        LV2_STATE_IS_POD);

    return LV2_STATE_SUCCESS;
}

// This runs in a lower priority thread
static LV2_Worker_Status
work(LV2_Handle instance,
    LV2_Worker_Respond_Function respond,
    LV2_Worker_Respond_Handle handle,
    uint32_t size,
    const void* data)
{
    batteur_plugin_t* self = (batteur_plugin_t*)instance;
    if (!data) {
        lv2_log_error(&self->logger, "[worker] Got an empty data.\n");
        return LV2_WORKER_ERR_UNKNOWN;
    }

    // Free the next beat, if any
    if (self->nextBeat) {
        batteur_free_beat(self->nextBeat);
        self->nextBeat = NULL;
    }

    const LV2_Atom* atom = (const LV2_Atom*)data;

    // Assume a path is for a beat
    if (atom->type == self->beat_description_uri || atom->type == self->atom_path_uri) {
        char* file_path = malloc(atom->size + 1);
        const char* atom_body = (const char*)LV2_ATOM_BODY_CONST(atom);
        batteur_beat_t* beat;

        file_path[0] = '\0';
        strncat(file_path, atom_body, atom->size);
        lv2_log_note(&self->logger, "Loading: %s\n", file_path);
        beat = batteur_load_beat(file_path);

        if (beat) {
            self->nextBeat = beat;
        }
        free(file_path);
    } else {
        lv2_log_error(&self->logger, "[worker] Got an unknown atom.\n");
        if (self->unmap)
            lv2_log_error(&self->logger,
                "URI: %s\n",
                self->unmap->unmap(self->unmap->handle, atom->type));
        return LV2_WORKER_ERR_UNKNOWN;
    }

    respond(handle, size, data);
    return LV2_WORKER_SUCCESS;
}

// This runs in the audio thread
static LV2_Worker_Status
work_response(LV2_Handle instance,
    uint32_t size,
    const void* data)
{
    UNUSED(size);
    batteur_plugin_t* self = (batteur_plugin_t*)instance;

    if (!data)
        return LV2_WORKER_ERR_UNKNOWN;

    const LV2_Atom* atom = (const LV2_Atom*)data;
    // Assume a path is for a beat
    if (atom->type == self->beat_description_uri || atom->type == self->atom_path_uri) {
        if (self->nextBeat) {
            batteur_stop(self->player);
            batteur_beat_t* beat = self->currentBeat;
            if (batteur_load(self->player, self->nextBeat)) {
                self->currentBeat = self->nextBeat;
                self->nextBeat = beat; // will be cleaned up on the next load
                batteur_skip_intro(self->player, true);

                // Update the file path
                // TODO: could swap the name with the beat to be honest...
                uint32_t size = atom->size;
                if (size >= (MAX_PATH_SIZE)) {
                    lv2_log_error(&self->logger, "File path size too big (%d)! Filename will be truncated.", size);
                    size = MAX_PATH_SIZE;
                }
                    
                const char* beat_file_path = LV2_ATOM_BODY_CONST(atom);
                strncpy(self->beat_file_path, beat_file_path, size);
                self->beat_file_path[size] = '\0';

                self->initial_countdown = 4;
                self->can_go_to_next_bar = false;
            }
        }
    } else {
        lv2_log_error(&self->logger, "[work_response] Got an unknown atom.\n");
        if (self->unmap)
            lv2_log_error(&self->logger,
                "URI: %s\n",
                self->unmap->unmap(self->unmap->handle, atom->type));
        return LV2_WORKER_ERR_UNKNOWN;
    }

    return LV2_WORKER_SUCCESS;
}

static const void*
extension_data(const char* uri)
{
    static const LV2_Options_Interface options = { lv2_get_options, lv2_set_options };
    static const LV2_State_Interface state = { save, restore };
    static const LV2_Worker_Interface worker = { work, work_response, NULL };

    // Advertise the extensions we support
    if (!strcmp(uri, LV2_OPTIONS__interface))
        return &options;
    else if (!strcmp(uri, LV2_STATE__interface))
        return &state;
    else if (!strcmp(uri, LV2_WORKER__interface))
        return &worker;

    return NULL;
}

static const LV2_Descriptor descriptor = {
    stomper_URI,
    instantiate,
    connect_port,
    activate,
    run,
    deactivate,
    cleanup,
    extension_data
};

LV2_SYMBOL_EXPORT
const LV2_Descriptor*
lv2_descriptor(uint32_t index)
{
    switch (index) {
    case 0:
        return &descriptor;
    default:
        return NULL;
    }
}
