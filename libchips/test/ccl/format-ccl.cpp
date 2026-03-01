#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "data/ccl/ccl_embeds.h"

extern "C" {
#include "formats.h"
}

struct TitleTimePair {
  char const* title;
  uint16_t time;
};

struct TitleTimePair pairs[] = {
  {"Key Pyramid", 200},
  {"Slip and Slide", 200},
  {"Present Company", 200},
  {"Block Party", 250},
  {"Facades", 250},
  {"When Insects Attack", 200},
  {"Under Pressure", 200},
  {"Switcheroo", 250},
  {"Swept Away", 250},
  {"Graduation", 400},
  {"Basketball", 250},
  {"Leave No Stone Unturned", 350},
  {"The Monster Cages", 300},
  {"Wedges", 250},
  {"Twister", 400},
  {"Tetragons", 350},
  {"Tiny", 0},
  {"Square Dancing", 300},
  {"Feel the Static", 400},
  {"Chip Suey", 450},
  {"Generic Ice Level", 198},
  {"Repair the Maze", 400},
  {"Circles", 250},
  {"Chip's Checkers", 350},
  {"Mind Lock", 150},
  {"Trafalgar Square", 200},
  {"Teleport Depot", 300},
  {"The Last Starfighter", 350},
  {"Sky High or Deep Down", 376},
  {"Button Brigade", 250},
  {"Quincunx", 175},
  {"Nitroglycerin", 350},
  {"Spitting Image", 0},
  {"Just a Bunch of Letters", 350},
  {"Mystery Wall", 450},
  {"Rhombus", 250},
  {"Habitat", 400},
  {"Heat Conductor", 600},
  {"Dig and Dig", 250},
  {"Sea Side", 0},
  {"Descending Ceiling", 200},
  {"Mughfe", 600},
  {"Gears", 250},
  {"Frozen Labyrinth", 500},
  {"Who's the Boss?", 300},
  {"Sapphire Cavern", 350},
  {"Bombs Away", 0},
  {"Sundance", 200},
  {"49 Cell", 490},
  {"The Grass Is Greener on the Other Side", 200},
  {"H2O Below 273 K", 300},
  {"The Bone", 350},
  {"Start at the End", 500},
  {"Mini Pyramid", 250},
  {"The Chambers", 400},
  {"Connect the Chips", 0},
  {"Key Farming", 350},
  {"Corral", 400},
  {"Asterisk", 0},
  {"Guard", 300},
  {"Highways", 450},
  {"Design Swap", 500},
  {"New Block in Town", 200},
  {"Chip Kart 64", 90},
  {"Squared in a Circle", 500},
  {"Klausswergner", 350},
  {"Booster Shots", 400},
  {"Flames and Ashes", 0},
  {"Double Diversion", 350},
  {"Juxtaposition", 500},
  {"Tree", 400},
  {"Breathing Room", 200},
  {"Occupied", 500},
  {"Traveler", 450},
  {"ToggleTank", 300},
  {"Funfair", 500},
  {"Shuttle Run", 60},
  {"Secret Passages", 600},
  {"Elevators", 0},
  {"Flipside", 450},
  {"Colors for Extreme", 0},
  {"Launch ", 150}, // don't ask about the title, I don't know why
  {"Ruined World", 0},
  {"Mining for Gold Keys", 600},
  {"Black Hole", 999},
  {"Starry Night", 350},
  {"Pluto", 700},
  {"Chip Block Galaxy", 0},
  {"Chip Grove City", 450},
  {"Bowling Alleys", 400},
  {"Roundabout", 400},
  {"The Shifting Maze", 999},
  {"Flame War", 350},
  {"Slime Forest", 600},
  {"Courtyard", 400},
  {"Going Underground", 450},
  {"Gate Keeper", 450},
  {"Rat Race", 400},
  {"Deserted Battlefield", 0},
  {"Loose Pocket", 350},
  {"Time Suspension", 0},
  {"Frozen in Time", 0},
  {"Portcullis", 0},
  {"Hotel Chip", 750},
  {"Tunnel Clearance", 300},
  {"Jailbird", 400},
  {"Paramecium Palace", 450},
  {"Exhibit Hall", 350},
  {"Green Clear", 500},
  {"Badlands", 0},
  {"Alternate Universe", 0},
  {"Carousel", 600},
  {"Teleport Trouble", 0},
  {"Comfort Zone", 350},
  {"California", 500},
  {"Communism", 600},
  {"Blobs on a Plane", 300},
  {"Runaway Train", 200},
  {"The Sewers", 450},
  {"Metal Harbor", 0},
  {"Chip Plank Galleon", 350},
  {"Jeepers Creepers", 700},
  {"The Very Hungry Caterpillar", 100},
  {"Utter Clutter", 777},
  {"Blockade", 250},
  {"Peek-a-Boo", 450},
  {"In the Pink", 500},
  {"Elemental Park", 800},
  {"Frogger", 300},
  {"Dynamite", 0},
  {"Easier Than It Looks", 130},
  {"Spumoni", 500},
  {"Steam Cleaner Simulator", 500},
  {"(Ir)reversible", 400},
  {"Culprit", 450},
  {"Whirlpool", 0},
  {"Thief Street", 200},
  {"Chip Alone", 600},
  {"Assassin", 300},
  {"Automatic (Caution) Doors", 999},
  {"Flush", 350},
  {"Bummbua Banubauabgv", 400},
  {"Amphibia", 0},
  {"The Ancient Temple", 600},
  {"Chance Time!", 250},
  {"Cineworld", 500},
  {"Thief, You've Taken All That Was Me", 999},
  {"The Snipers", 450},
  {"Clubhouse", 600},
};

namespace {
  TEST(FormatCCL, LoadCCLP1) {
    Result_LevelSetPtr res = parse_ccl(CCLP1_ccl, sizeof(CCLP1_ccl));
    EXPECT_TRUE(res.success);

    LevelSet* set = res.value;
    EXPECT_EQ(LevelSet_get_levels_n(set), 149);

    for (size_t i = 0; i < 149; i += 1) {
      EXPECT_STREQ(LevelMetadata_get_title(LevelSet_get_level(set, i)), pairs[i].title);
      EXPECT_EQ(LevelMetadata_get_time_limit(LevelSet_get_level(set, i)), pairs[i].time);

      Result_LevelPtr level_res = LevelMetadata_make_level(LevelSet_get_level(set, i), &ms_logic);
      EXPECT_TRUE(level_res.success);
      Level_free(level_res.value);
    }
    LevelSet_free(set);
  }

  TEST(FormatTws, EmptyFile) {
    uint8_t data[0];
    Result_LevelSetPtr ccl = parse_ccl(data, 0);
    ASSERT_FALSE(ccl.success);
    free(ccl.error);
  }

  TEST(FormatTws, NullFile) {
    Result_LevelSetPtr ccl = parse_ccl(NULL, 100);
    ASSERT_FALSE(ccl.success);
    free(ccl.error);
  }
}
