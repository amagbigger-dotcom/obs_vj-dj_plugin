#pragma once

#include <string>
#include <vector>

void vdj_midi_init();
void vdj_midi_shutdown();
bool vdj_midi_is_connected();
const char *vdj_midi_get_device_name();

// Two-way MIDI Clock
void vdj_midi_set_clock_send(bool enable);
bool vdj_midi_is_clock_send();

std::vector<std::string> vdj_midi_get_output_devices();
bool vdj_midi_open_output_device(int device_id);
