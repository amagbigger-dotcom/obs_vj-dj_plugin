#pragma once

#include "controllers/midi_types.hpp"
#include "controllers/midi_mapping.hpp"
#include <string>
#include <vector>

void vdj_midi_init();
void vdj_midi_shutdown();
bool vdj_midi_is_connected();
const char *vdj_midi_get_device_name();

// Controller Profile & Dispatcher
void vdj_midi_set_profile(const MidiControllerProfile &p);
MidiControllerProfile vdj_midi_get_profile();
void vdj_midi_start_learn(MidiAction action, int deck);
void vdj_midi_cancel_learn();
bool vdj_midi_is_learning();

// Two-way MIDI Clock
void vdj_midi_set_clock_send(bool enable);
bool vdj_midi_is_clock_send();

std::vector<std::string> vdj_midi_get_input_devices();
bool vdj_midi_open_input_device(int device_id);

std::vector<std::string> vdj_midi_get_output_devices();
bool vdj_midi_open_output_device(int device_id);
void vdj_midi_send_short_msg(uint8_t status, uint8_t data1, uint8_t data2);

