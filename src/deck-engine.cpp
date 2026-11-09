#include "deck-engine.hpp"

#include <obs-module.h>
#include <algorithm>
#include <cmath>

namespace {
DeckPair *g_decks = nullptr;

// A beat-grid és BPM-elemzés még nincs implementálva.
// Az Auto Loop ezért 120 BPM referencia-tempót használ.
constexpr double kReferenceBeatMs = 500.0;
constexpr int kLoopBeatCounts[] = {0, 1, 2, 4, 8, 16, 32, 64};
}

DeckPlayer::DeckPlayer(const char *source_name, const char *display_name)
{
    obs_data_t *settings = obs_data_create();
    if (!settings) {
        blog(LOG_ERROR, "[obs-vj-mixer] Could not allocate settings for '%s'", source_name);
        return;
    }

    obs_data_set_bool(settings, "looping", false);
    obs_data_set_bool(settings, "close_when_inactive", false);
    obs_data_set_bool(settings, "restart_on_activate", false);
    obs_data_set_bool(settings, "clear_on_media_end", false);
    obs_data_set_int(settings, "speed_percent", 100);

    source_ = obs_source_create_private("ffmpeg_source", source_name, settings);
    obs_data_release(settings);

    if (!source_) {
        blog(LOG_WARNING, "[obs-vj-mixer] Could not create ffmpeg_source '%s' (%s)",
             source_name, display_name);
        return;
    }

    obs_source_inc_showing(source_);
}
