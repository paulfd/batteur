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

#define DEFAULT_SFZ_FILE ""
#define batteur_URI "https://github.com/paulfd/batteur"
#define batteur__beatDescription "https://github.com/paulfd/batteur:beatDescription"
#define batteur__mainSwitch "https://github.com/paulfd/batteur:mainSwitch"
#define batteur__status "https://github.com/paulfd/batteur:status"
#define MAIN_SWITCH_ON "Switch on!"
#define MAIN_SWITCH_OFF "Switch off!"
#define CHANNEL_MASK 0x0F
#define NOTE_ON 0x90
#define NOTE_OFF 0x80
#define MIDI_CHANNEL(byte) (byte & CHANNEL_MASK)
#define MIDI_STATUS(byte) (byte & ~CHANNEL_MASK)
#define MAX_BLOCK_SIZE 8192
#define MAX_PATH_SIZE 1024
#define UNUSED(x) (void)(x)
#define SWITCH_DURATION 0.75
#define DEFAULT_ACCENT_NOTE 49
#define DEFAULT_ACCENT_VELOCITY 100

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
    const float* output_channel_p;
    const float* main_p;
    const float* accent_p;
    const float* accent_note_p;

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
    LV2_URID time_position_uri;
    LV2_URID time_bar_beat_uri;
    LV2_URID time_bpm_uri;
    LV2_URID time_speed_uri;
    LV2_URID state_changed_uri;
    LV2_URID beat_description_uri;
    LV2_URID main_switch_uri;
    LV2_URID status_uri;

    // Sfizz related data
    // sfizz_synth_t *synth;
    batteur_beat_t* currentBeat;
    batteur_beat_t* nextBeat;
    batteur_player_t* player;
    bool expect_nominal_block_length;
    float main_switch_status;
    bool accent_pressed;
    char beat_file_path[MAX_PATH_SIZE];
    int max_block_size;
    int accent_note;
    bool bpm_set_by_host;
    float bpm;
    float speed;
    float beat;
    int64_t last_main_up;
    int64_t last_main_down;
    double sample_rate;
} batteur_plugin_t;

enum {
    INPUT_PORT = 0,
    OUTPUT_PORT,
    MAIN_PORT,
    ACCENT_PORT,
    ACCENT_NOTE_PORT,
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
    self->state_changed_uri = map->map(map->handle, LV2_STATE__StateChanged);
    self->time_position_uri = map->map(map->handle, LV2_TIME__Position);
    self->time_bar_beat_uri = map->map(map->handle, LV2_TIME__barBeat);
    self->time_bpm_uri = map->map(map->handle, LV2_TIME__beatsPerMinute);
    self->time_speed_uri = map->map(map->handle, LV2_TIME__speed);
    self->beat_description_uri = map->map(map->handle, batteur__beatDescription);
    self->main_switch_uri = map->map(map->handle, batteur__mainSwitch);
    self->status_uri = map->map(map->handle, batteur__status);
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
    case ACCENT_PORT:
        self->accent_p = (const float*)data;
        break;
    case ACCENT_NOTE_PORT:
        self->accent_note_p = (const float*)data;
        break;
    default:
        break;
    }
}

static void
batteur_callback(int delay, uint8_t number, uint8_t value, void* cbdata)
{
    batteur_plugin_t* self = (batteur_plugin_t*)cbdata;
    
    uint8_t msg[3] = { 
        value > 0 ? NOTE_ON : NOTE_OFF,
        number,
        value
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
    self->accent_pressed = false;
    self->last_main_up = 0;
    self->beat = 0.0f;
    self->bpm_set_by_host = false;
    self->bpm = 120.0f;
    self->speed = 1.0f;
    self->main_switch_status = 0.0f;
    self->accent_note = DEFAULT_ACCENT_NOTE;
    self->last_main_down = -(int64_t)(SWITCH_DURATION * rate);
    self->nextBeat = NULL;

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
        free(self);
        return NULL;
    }

    // The worker feature is required
    if (!self->worker) {
        lv2_log_error(&self->logger, "Worker feature not found, aborting...\n");
        free(self);
        return NULL;
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
        free(self);
        return NULL;
    }

    self->player = batteur_new();
    batteur_note_cb(self->player, &batteur_callback, (void*)self);
    return (LV2_Handle)self;
}

static void
cleanup(LV2_Handle instance)
{
    batteur_plugin_t* self = (batteur_plugin_t*)instance;
    batteur_free_beat(self->currentBeat);
    batteur_free_beat(self->nextBeat);
    batteur_free(self->player);
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
batteur_process_midi_event(batteur_plugin_t* self, const LV2_Atom_Event* ev)
{
    const uint8_t* const msg = (const uint8_t*)(ev + 1);
    switch (lv2_midi_message_type(msg)) {
    case LV2_MIDI_MSG_NOTE_ON:
        lv2_log_note(&self->logger,
            "[process_midi] Received note on %d/%d at time %ld\n", msg[0], msg[1], ev->time.frames);
        break;
    case LV2_MIDI_MSG_NOTE_OFF:
        lv2_log_note(&self->logger,
            "[process_midi] Received note off %d/%d at time %ld\n", msg[0], msg[1], ev->time.frames);
        break;
    case LV2_MIDI_MSG_CONTROLLER:
        lv2_log_note(&self->logger,
            "[process_midi] Received CC %d/%d at time %ld\n", msg[0], msg[1], ev->time.frames);
        break;
    default:
        break;
    }
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
send_status(batteur_plugin_t* self)
{
    LV2_Atom_Forge_Frame frame;
    lv2_atom_forge_frame_time(&self->forge, 0);
    lv2_atom_forge_object(&self->forge, &frame, 0, self->patch_set_uri);
    lv2_atom_forge_key(&self->forge, self->patch_property_uri);
    lv2_atom_forge_urid(&self->forge, self->status_uri);
    lv2_atom_forge_key(&self->forge, self->patch_value_uri);
    if (self->main_switch_status)
        lv2_atom_forge_string(&self->forge, MAIN_SWITCH_ON, strlen(MAIN_SWITCH_ON));
    else
        lv2_atom_forge_string(&self->forge, MAIN_SWITCH_OFF, strlen(MAIN_SWITCH_OFF));

    lv2_atom_forge_pop(&self->forge, &frame);
}

static void
main_switch_event(batteur_plugin_t* self, float switch_status)
{
    if (switch_status == 1.0f) {
        self->last_main_up = 0;
    } else if (switch_status == 0.0f) {
        const double since_switch_pressed = 
            (double)(self->last_main_up) / self->sample_rate;

        const double since_last_down = 
            (double)(self->last_main_down) / self->sample_rate;

        lv2_log_note(&self->logger, 
                    "[run] Main switch down (%.3f s since switch pressed)\n",
                    since_switch_pressed);
        
        if (batteur_playing(self->player)) {
            if (since_last_down < SWITCH_DURATION) {
                lv2_log_note(&self->logger, "[run] Stop\n");
                batteur_stop(self->player);
            } else if (since_switch_pressed < SWITCH_DURATION) {
                lv2_log_note(&self->logger, "[run] Fill in\n");
                batteur_fill_in(self->player);
            } else {
                lv2_log_note(&self->logger, "[run] Next\n");
                batteur_next(self->player);
            }
        } else {
            lv2_log_note(&self->logger, "[run] Play\n");
            batteur_start(self->player);
        }
        
        self->last_main_down = 0;
    }
}

static void
beat_description_event(batteur_plugin_t* self, const LV2_Atom* atom)
{
    const uint32_t original_atom_size = lv2_atom_total_size((const LV2_Atom*)atom);
    const uint32_t null_terminated_atom_size = original_atom_size + 1;
    char atom_buffer[null_terminated_atom_size];
    memcpy(&atom_buffer, atom, original_atom_size);
    atom_buffer[original_atom_size] = 0; // Null terminate the string for safety
    LV2_Atom* file_path = (LV2_Atom*)&atom_buffer;
    file_path->type = self->beat_description_uri;

    // If the parameter is different from the current one we send it through
    if (strcmp(self->beat_file_path, LV2_ATOM_BODY_CONST(file_path)))
        self->worker->schedule_work(self->worker->handle, 
                                    null_terminated_atom_size,
                                    file_path);

    lv2_log_note(&self->logger,
                "[handle_object] Received a description file: %s\n",
                (char*)LV2_ATOM_BODY_CONST(file_path));
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
        lv2_log_warning(&self->logger, "Got an Patch GET with no body.\n");
        send_file_path(self);
        send_status(self);
    } else if (property->body == self->beat_description_uri) {
        lv2_log_warning(&self->logger, "Got an Patch GET for the beat description.\n");
        send_file_path(self);
    } else if (property->body == self->status_uri) {
        lv2_log_warning(&self->logger, "Got an Patch GET for the status.\n");
        send_status(self);
    }
}

static void
set_tempo(batteur_plugin_t* self, const LV2_Atom_Object* obj)
{
    LV2_Atom *beat = NULL;
    LV2_Atom *bpm = NULL;
    LV2_Atom *speed = NULL;
    lv2_atom_object_get(obj,
                        self->time_bar_beat_uri, &beat,
                        self->time_bpm_uri, &bpm,
                        self->time_speed_uri, &speed,
                        NULL);

    if (bpm && bpm->type == self->atom_float_uri) {
        // Tempo changed, update BPM
        self->bpm = ((LV2_Atom_Float*)bpm)->body;
        batteur_set_tempo(self->player, self->bpm);
        self->bpm_set_by_host = true;
        lv2_log_note(&self->logger, "BPM changed to: %.4f\n", self->bpm);
    }

    if (speed && speed->type == self->atom_float_uri) {
        // Speed changed, e.g. 0 (stop) to 1 (play)
        self->speed = ((LV2_Atom_Float*)speed)->body;
        if (self->speed == 0.0f)
            batteur_all_off(self->player);
        lv2_log_note(&self->logger, "Speed changed to: %.4f\n", self->speed);
    }

    if (beat && beat->type == self->atom_float_uri) {
        self->beat = ((LV2_Atom_Float*)beat)->body;
        lv2_log_note(&self->logger, "Beat not handled: %.4f\n", self->beat);
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
            } else if (obj->body.otype == self->time_position_uri) {
                set_tempo(self, obj);
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
            batteur_process_midi_event(self, ev);
        }
    }

    if (*self->main_p != self->main_switch_status) {
        main_switch_event(self, *self->main_p);
        self->main_switch_status = *self->main_p;
        send_status(self);
    }

    if (*self->accent_p) {
        if (!self->accent_pressed) {
            lv2_log_warning(&self->logger, "Accent!\n");
            batteur_callback(0, (uint8_t)*self->accent_note_p, DEFAULT_ACCENT_VELOCITY, self);
            self->accent_pressed = true;
        }
    } else {
        self->accent_pressed = false;
    }

    self->last_main_up += sample_count;
    self->last_main_down += sample_count;
    batteur_tick(self->player, sample_count);
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
    if (value) {
        // lv2_log_note(&self->logger, "Restoring the file %s\n", (const char*)value);
        batteur_beat_t* beat = batteur_load_beat((const char *)value);
        if (beat) {
            strcpy(self->beat_file_path, (const char *)value);
            self->currentBeat = beat;
            batteur_load(self->player, beat);
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
    if (atom->type == self->beat_description_uri) {
        const char* file_path = LV2_ATOM_BODY_CONST(atom);
        // lv2_log_note(&self->logger, "[work] Loading file: %s\n", file_path);
        batteur_beat_t* beat = batteur_load_beat(file_path);
        if (beat) {
            self->nextBeat = beat;
        }
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
    if (atom->type == self->beat_description_uri) {
        const char* beat_file_path = LV2_ATOM_BODY_CONST(atom);
        if (self->nextBeat) {
            batteur_beat_t* beat = self->currentBeat;
            if (batteur_load(self->player, self->nextBeat)) {
                self->currentBeat = self->nextBeat;
                self->nextBeat = beat; // will be cleaned up on the next load
                if (self->bpm_set_by_host)
                    batteur_set_tempo(self->player, self->bpm);

                strcpy(self->beat_file_path, beat_file_path);
            }
        }
        // lv2_log_note(&self->logger, "[work_response] File changed to: %s\n", self->beat_file_path);
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
    batteur_URI,
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
