#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "data/ccl/ccl_embeds.h"
#include "data/tws/tws_embeds.h"

extern "C" {
#include "formats.h"
#include "format-tws.h"
#include "logic.h"
}

struct LevelsetTwssetPair {
  LevelSet* set;
  TWSSet* tws;
};

typedef std::optional<LevelsetTwssetPair> LevelsetTwssetPairOptional;

namespace {
  LevelsetTwssetPairOptional loadsets(uint8_t const* levelset, size_t levelset_size, uint8_t const* tws, size_t tws_size) {
    LevelsetTwssetPair pair = {};

    Result_LevelSetPtr res = parse_ccl(levelset, levelset_size);
    EXPECT_TRUE(res.success);
    if (!res.success) {
      eprintf("%s\n", res.error);
      return std::nullopt;
    }
    pair.set = res.value;

    Result_TWSSetPtr tws_res = parse_tws(tws, tws_size);
    EXPECT_TRUE(tws_res.success);
    if (!tws_res.success) {
      eprintf("%s\n", tws_res.error);
      return std::nullopt;
    }
    pair.tws = tws_res.value;
    return pair;
  }

  void freeset(LevelsetTwssetPair pair) {
    LevelSet_free(pair.set);
    TWSSet_free(pair.tws);
  }

  void print_moves(uint16_t level_num, GameInputList const* move_list, uint32_t num_ticks) {
    const char moves_chars[] = {
      [DIRECTION_NIL] = '-',
      [DIRECTION_NORTH] = 'N',
      [DIRECTION_WEST] = 'W',
      [DIRECTION_SOUTH] = 'S',
      [DIRECTION_EAST] = 'E',
      [DIRECTION_NORTH | DIRECTION_WEST] = 'Q',
      [DIRECTION_SOUTH | DIRECTION_WEST] = 'Z',
      [DIRECTION_NORTH | DIRECTION_EAST] = 'R',
      [DIRECTION_SOUTH | DIRECTION_EAST] = 'V',
    };

    printf("%u: ", level_num);
    for (size_t i = 0; i < num_ticks; i += 1) {
      putc(moves_chars[move_list->inputs[i]], stdout);
    }
    putc('\n', stdout);
  }

  void testset(LevelsetTwssetPair pair) {
    EXPECT_EQ(LevelSet_get_levels_n(pair.set), TWSSet_get_solutions_n(pair.tws));
    if (pair.set->levels_n != pair.tws->solutions_n) {
      return;
    }

    for (size_t i = 0; i < pair.set->levels_n; i += 1) {
      Result_LevelPtr level_res;
      if (TWSSet_get_ruleset(pair.tws) == Ruleset_MS) {
        level_res = LevelMetadata_make_level(&pair.set->levels[i], &ms_logic);
      } else if (TWSSet_get_ruleset(pair.tws) == Ruleset_Lynx) {
        level_res = LevelMetadata_make_level(&pair.set->levels[i], &lynx_logic);
      }
      EXPECT_TRUE(level_res.success);
      Level* level = level_res.value;

      TWSMetadata const* solution = TWSSet_get_level_solution(pair.tws, i + 1);
      Level_set_init_step_parity(level, solution->init_step_parity);
      Level_set_rff_dir(level, solution->rff_dir);
      Prng_init_seeded(Level_get_prng_ptr(level), solution->prng_seed);

      Result_GameInputList res = TWSMetadata_prepare_inputs(solution);
      EXPECT_TRUE(res.success);
      GameInputList input_list = res.value;
      if (input_list.count < TWSMetadata_get_length(solution)) {
        printf("%d:\n", solution->level_num);
      }
      EXPECT_GE(input_list.count, TWSMetadata_get_length(solution));
      if (input_list.count >= TWSMetadata_get_length(solution)) {
        for (size_t j = 0; j < input_list.count; j += 1) {
          Level_set_game_input(level, GameInputList_get_input(&input_list, j));
          Level_tick(level);
        }
        if (Level_get_win_state(level) != TRIRES_SUCCESS) {
          if (Level_get_ruleset(level)->id == Ruleset_MS) { //you can thank MS for slides into the exit being weird
            Level_set_game_input(level, GameInputList_get_input(&input_list, DIRECTION_NIL));
            Level_tick(level);
          }
          if (Level_get_win_state(level) != TRIRES_SUCCESS) {
            print_moves(TWSMetadata_get_level_num(solution), &input_list, TWSMetadata_get_length(solution));
          }
        }
        EXPECT_EQ(Level_get_win_state(level), TRIRES_SUCCESS);
        EXPECT_EQ(Level_get_current_tick(level) + Level_get_time_offset(level), TWSMetadata_get_length(solution));
      }
      GameInputList_free(&input_list);
      Level_free(level);
    }
    freeset(pair);
    return;
  }

  void load_test_set(uint8_t const* levelset, size_t levelset_size, uint8_t const* tws, size_t tws_size) {
    LevelsetTwssetPairOptional pair = loadsets(levelset, levelset_size, tws, tws_size);
    if (!pair.has_value()) return;
    testset(pair.value());
  }

  TEST(MSCCLP, LoadAndPlayCCLP1) {
    load_test_set(CCLP1_ccl, sizeof(CCLP1_ccl), public_CCLP1_tws, sizeof(public_CCLP1_tws));
  }

  TEST(LynxCCLP, LoadAndPlayCCLP1) {
    load_test_set(CCLP1_ccl, sizeof(CCLP1_ccl), public_CCLP1_lynx_tws, sizeof(public_CCLP1_lynx_tws));
  }

  TEST(MSCCLP, LoadAndPlayCCLP2) {
    load_test_set(CCLP2_ccl, sizeof(CCLP2_ccl), public_CCLP2_tws, sizeof(public_CCLP2_tws));
  }

  TEST(LynxCCLP, LoadAndPlayCCLXP2) {
    load_test_set(CCLXP2_ccl, sizeof(CCLXP2_ccl), public_CCLXP2_tws, sizeof(public_CCLXP2_tws));
  }

  TEST(MSCCLP, LoadAndPlayCCLP3) {
    load_test_set(CCLP3_ccl, sizeof(CCLP3_ccl), public_CCLP3_tws, sizeof(public_CCLP3_tws));
  }

  TEST(LynxCCLP, LoadAndPlayCCLP3) {
    load_test_set(CCLP3_ccl, sizeof(CCLP3_ccl), public_CCLP3_lynx_tws, sizeof(public_CCLP3_lynx_tws));
  }

  TEST(MSCCLP, LoadAndPlayCCLP4) {
    load_test_set(CCLP4_ccl, sizeof(CCLP4_ccl), public_CCLP4_tws, sizeof(public_CCLP4_tws));
  }

  TEST(LynxCCLP, LoadAndPlayCCLP4) {
    load_test_set(CCLP4_ccl, sizeof(CCLP4_ccl), public_CCLP4_lynx_tws, sizeof(public_CCLP4_lynx_tws));
  }

  // public TWSes for CCLP5 aren't full yet and as such aren't really good for this
  // TEST(CCLP5TWS, LoadAndPlayMS) {
  //   load_test_set(CCLP5_ccl, sizeof(CCLP5_ccl), public_CCLP5_tws, sizeof(public_CCLP5_tws));
  // }
  //
  // TEST(CCLP5TWS, LoadAndPlayLynx) {
  //   load_test_set(CCLP5_ccl, sizeof(CCLP5_ccl), public_CCLP5_lynx_tws, sizeof(public_CCLP5_lynx_tws));
  // }
}
