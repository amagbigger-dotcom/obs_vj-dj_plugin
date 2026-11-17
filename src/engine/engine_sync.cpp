#include "engine/engine_sync.hpp"

double EngineSync::calculateEffectiveMasterBpm(int mode,
                                               double master_clock_bpm,
                                               bool deck_a_playing,
                                               double deck_a_bpm,
                                               bool deck_b_playing,
                                               double deck_b_bpm) noexcept
{
    if (mode == SYNC_MASTER_CLOCK) {
        return master_clock_bpm;
    }
    if (mode == SYNC_MASTER_DECK_A && deck_a_bpm > 30.0) {
        return deck_a_bpm;
    }
    if (mode == SYNC_MASTER_DECK_B && deck_b_bpm > 30.0) {
        return deck_b_bpm;
    }
    if (mode == SYNC_MASTER_EXT_MIDI) {
        return master_clock_bpm;
    }

    // Ha nincs explicit master, az éppen játszó deck tempóját követjük
    if (deck_a_playing && deck_a_bpm > 30.0) {
        return deck_a_bpm;
    }
    if (deck_b_playing && deck_b_bpm > 30.0) {
        return deck_b_bpm;
    }
    return master_clock_bpm;
}

double EngineSync::calculateTargetBpmForSync(bool target_is_master,
                                             int master_mode,
                                             double master_clock_bpm,
                                             bool other_is_master,
                                             bool other_is_playing,
                                             double other_effective_bpm,
                                             bool other_has_track,
                                             double other_base_bpm) noexcept
{
    // Ha a cél deck a MASTER, a saját tempója a referencia: tilos elállítani! (0.0 jelzi, hogy nincs tempóváltás)
    if (target_is_master) {
        return 0.0;
    }

    if (other_is_master && other_effective_bpm > 30.0) {
        return other_effective_bpm;
    }

    if (master_mode == SYNC_MASTER_CLOCK || master_mode == SYNC_MASTER_EXT_MIDI) {
        return master_clock_bpm;
    }

    if (other_is_playing && other_effective_bpm > 30.0) {
        return other_effective_bpm;
    }

    if (other_has_track && other_effective_bpm > 30.0) {
        return other_effective_bpm;
    }

    if (other_has_track && other_base_bpm > 30.0) {
        return other_base_bpm;
    }

    return master_clock_bpm;
}
