#include <gtest/gtest.h>
#include <gmock/gmock.h>

extern "C" {
#include "formats.h"
#include "format-tws.h"
#include "logic.h"
}

namespace {
#include "data/ccl/CCLP1.ccl.h"
#include "data/tws/public_CHIPS.dac.tws.h"
#include "data/tws/public_CHIPS_lynx.dac.tws.h"
  TEST(CCLP1TWS, LoadAndPlayMS) {
    Result_LevelSetPtr res = parse_ccl(CCLP1_ccl, sizeof(CCLP1_ccl));
    EXPECT_TRUE(res.success);
    LevelSet* set = res.value;

    Result_TWSSetPtr tws_res = parse_tws(public_CHIPS_tws, sizeof(public_CHIPS_tws));
    EXPECT_TRUE(tws_res.success);
    TWSSet* tws = tws_res.value;

    for (size_t i = 0; i < set->levels_n; i++) {
      Result_LevelPtr level_res = LevelMetadata_make_level(&set->levels[i], &ms_logic);
      EXPECT_TRUE(level_res.success);
      Level* level = level_res.value;

      TWSMetadata* solution = &tws->solutions[i];
      for (size_t j = 0; j < solution->num_ticks; j++) {
        level->game_input = solution->inputs[j];
        ms_logic.tick_level(level);
      }
      EXPECT_TRUE(level->level_complete);
      Level_free(level);
    }
    LevelSet_free(set);
    TWSSet_free(tws);
  }

  TEST(CCLP1TWS, LoadAndPlayLynx) {
    Result_LevelSetPtr res = parse_ccl(CCLP1_ccl, sizeof(CCLP1_ccl));
    EXPECT_TRUE(res.success);
    LevelSet* set = res.value;

    Result_TWSSetPtr tws_res = parse_tws(public_CHIPS_lynx_tws, sizeof(public_CHIPS_lynx_tws));
    EXPECT_TRUE(tws_res.success);
    TWSSet* tws = tws_res.value;

    for (size_t i = 0; i < set->levels_n; i++) {
      Result_LevelPtr level_res = LevelMetadata_make_level(&set->levels[i], &lynx_logic);
      EXPECT_TRUE(level_res.success);
      Level* level = level_res.value;

      TWSMetadata* solution = &tws->solutions[i];
      for (size_t j = 0; j < solution->num_ticks; j++) {
        level->game_input = solution->inputs[j];
        lynx_logic.tick_level(level);
      }
      EXPECT_TRUE(level->level_complete);
    }
  }
}
