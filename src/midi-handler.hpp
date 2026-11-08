#pragma once

void vdj_midi_init();
void vdj_midi_shutdown();
bool vdj_midi_is_connected();
const char *vdj_midi_get_device_name();
