#pragma once

#include <cstdint>
#include <cmath>
#include <algorithm>

enum MasterSyncMode {
    SYNC_MASTER_CLOCK = 0,
    SYNC_MASTER_DECK_A = 1,
    SYNC_MASTER_DECK_B = 2,
    SYNC_MASTER_EXT_MIDI = 3
};

class EngineSync {
public:
    static double calculateEffectiveMasterBpm(int mode,
                                              double master_clock_bpm,
                                              bool deck_a_playing,
                                              double deck_a_bpm,
                                              bool deck_b_playing,
                                              double deck_b_bpm) noexcept;

    static double calculateTargetBpmForSync(bool target_is_master,
                                            int master_mode,
                                            double master_clock_bpm,
                                            bool other_is_master,
                                            bool other_is_playing,
                                            double other_effective_bpm,
                                            bool other_has_track,
                                            double other_base_bpm) noexcept;
};
