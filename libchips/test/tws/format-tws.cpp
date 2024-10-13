#include <gtest/gtest.h>
#include <gmock/gmock.h>

extern "C" {
#include "format-tws.h"
}

namespace {

  TEST(FormatTws, EmptyFile) {
    uint8_t data[0];
    Result_TWSSetPtr tws = parse_tws(data, 0);
    ASSERT_FALSE(tws.success);
  }
  TEST(FormatTws, NullFile) {
    Result_TWSSetPtr tws = parse_tws(NULL, 100);
    ASSERT_FALSE(tws.success);
  }

#include "data/tws/example.tws.h"
  TEST(FormatTws, Example) {
    Result_TWSSetPtr tws_res = parse_tws(example_tws, sizeof(example_tws));
    EXPECT_TRUE(tws_res.success);
    TWSSet* set = tws_res.value;
    EXPECT_EQ(set->ruleset, Ruleset_MS);
    EXPECT_EQ(set->solutions_n, 2);
    EXPECT_EQ(set->solutions_allocated, 2);

    EXPECT_EQ(set->solutions[0].level_num, 1);
    EXPECT_EQ(set->solutions[0].prng_seed, 342566057);
    EXPECT_EQ(set->solutions[0].num_ticks, 398);
    EXPECT_EQ(set->solutions[0].other_flags, 0);
    EXPECT_EQ(set->solutions[0].step_value, 0);
    EXPECT_EQ(set->solutions[0].slide_direction, 0);

    EXPECT_EQ(set->solutions[1].level_num, 2);
    EXPECT_EQ(set->solutions[1].num_ticks, 0);
    EXPECT_EQ(set->solutions[1].inputs, nullptr);

    EXPECT_EQ(set->solutions[0].num_ticks, std::size(example_inputs));
    for (size_t i = 0; i < set->solutions[0].num_ticks; i++) {
      GameInput input = set->solutions[0].inputs[i];
      GameInput example_input = example_inputs[i];
      // printf("%d : %d\n", input, example_input);
      EXPECT_EQ(input, example_input);
    }

    TWSSet_free(set);
  }

#include "data/tws/public_CHIPS.dac.tws.h"
  TEST(FormatTws, public_CHIPS) {
    Result_TWSSetPtr tws_res = parse_tws(public_CHIPS_tws, sizeof(public_CHIPS_tws));
    EXPECT_TRUE(tws_res.success);
    TWSSet* set = tws_res.value;
    EXPECT_EQ(set->ruleset, Ruleset_MS);
    EXPECT_EQ(set->solutions_n, 149);
    EXPECT_EQ(set->solutions_allocated, 149);

    EXPECT_STREQ(set->set_name, "public_CHIPS.dac");

    TWSSet_free(set);
  }

}