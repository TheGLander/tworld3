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
    EXPECT_EQ(set->solutions_n, 1);
    EXPECT_EQ(set->solutions_allocated, 1);

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