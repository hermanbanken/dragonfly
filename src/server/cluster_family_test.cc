// Copyright 2023, DragonflyDB authors.  All rights reserved.
// See LICENSE for licensing terms.
//

#include <absl/flags/reflection.h>
#include <gmock/gmock-matchers.h>

#include <string>
#include <string_view>

#include "base/gtest.h"
#include "base/logging.h"
#include "facade/facade_test.h"
#include "server/test_utils.h"

namespace dfly {
namespace {

using namespace std;
using testing::HasSubstr;

class ClusterFamilyTest : public BaseFamilyTest {
 public:
  ClusterFamilyTest() {
    auto* flag = absl::FindCommandLineFlag("cluster_mode");
    CHECK_NE(flag, nullptr);
    string error;
    CHECK(flag->ParseFrom("yes", &error));
  }

 protected:
  static constexpr string_view kInvalidConfiguration = "Invalid cluster configuration";
};

TEST_F(ClusterFamilyTest, ClusterConfigInvalidJSON) {
  EXPECT_THAT(Run({"dflycluster", "config", "invalid JSON"}),
              ErrArg("Invalid JSON cluster config"));

  string cluster_info = Run({"cluster", "info"}).GetString();
  EXPECT_THAT(cluster_info, HasSubstr("cluster_state:fail"));
  EXPECT_THAT(cluster_info, HasSubstr("cluster_slots_assigned:0"));
  EXPECT_THAT(cluster_info, HasSubstr("cluster_slots_ok:0"));
  EXPECT_THAT(cluster_info, HasSubstr("cluster_known_nodes:0"));
  EXPECT_THAT(cluster_info, HasSubstr("cluster_size:0"));
}

TEST_F(ClusterFamilyTest, ClusterConfigInvalidConfig) {
  EXPECT_THAT(Run({"dflycluster", "config", "[]"}), ErrArg(kInvalidConfiguration));

  string cluster_info = Run({"cluster", "info"}).GetString();
  EXPECT_THAT(cluster_info, HasSubstr("cluster_state:fail"));
  EXPECT_THAT(cluster_info, HasSubstr("cluster_slots_assigned:0"));
  EXPECT_THAT(cluster_info, HasSubstr("cluster_slots_ok:0"));
  EXPECT_THAT(cluster_info, HasSubstr("cluster_known_nodes:0"));
  EXPECT_THAT(cluster_info, HasSubstr("cluster_size:0"));
}

TEST_F(ClusterFamilyTest, ClusterConfigInvalidMissingSlots) {
  EXPECT_THAT(Run({"dflycluster", "config", R"json(
      [
        {
          "slot_ranges": [
            {
              "start": 0,
              "end": 100
            }
          ],
          "master": {
            "id": "abcd1234",
            "ip": "10.0.0.1",
            "port": 7000
          },
          "replicas": []
        }
      ])json"}),
              ErrArg(kInvalidConfiguration));

  string cluster_info = Run({"cluster", "info"}).GetString();
  EXPECT_THAT(cluster_info, HasSubstr("cluster_state:fail"));
  EXPECT_THAT(cluster_info, HasSubstr("cluster_slots_assigned:0"));
  EXPECT_THAT(cluster_info, HasSubstr("cluster_slots_ok:0"));
  EXPECT_THAT(cluster_info, HasSubstr("cluster_known_nodes:0"));
  EXPECT_THAT(cluster_info, HasSubstr("cluster_size:0"));
}

TEST_F(ClusterFamilyTest, ClusterConfigInvalidOverlappingSlots) {
  EXPECT_THAT(Run({"dflycluster", "config", R"json(
      [
        {
          "slot_ranges": [
            {
              "start": 0,
              "end": 1000
            }
          ],
          "master": {
            "id": "abcd1234",
            "ip": "10.0.0.1",
            "port": 7000
          },
          "replicas": []
        },
        {
          "slot_ranges": [
            {
              "start": 800,
              "end": 16383
            }
          ],
          "master": {
            "id": "abcd1234",
            "ip": "10.0.0.1",
            "port": 7000
          },
          "replicas": []
        }
      ])json"}),
              ErrArg(kInvalidConfiguration));

  string cluster_info = Run({"cluster", "info"}).GetString();
  EXPECT_THAT(cluster_info, HasSubstr("cluster_state:fail"));
  EXPECT_THAT(cluster_info, HasSubstr("cluster_slots_assigned:0"));
  EXPECT_THAT(cluster_info, HasSubstr("cluster_slots_ok:0"));
  EXPECT_THAT(cluster_info, HasSubstr("cluster_known_nodes:0"));
  EXPECT_THAT(cluster_info, HasSubstr("cluster_size:0"));
}

TEST_F(ClusterFamilyTest, ClusterConfigNoReplicas) {
  EXPECT_EQ(Run({"dflycluster", "config", R"json(
      [
        {
          "slot_ranges": [
            {
              "start": 0,
              "end": 16383
            }
          ],
          "master": {
            "id": "abcd1234",
            "ip": "10.0.0.1",
            "port": 7000
          },
          "replicas": []
        }
      ])json"}),
            "OK");

  string cluster_info = Run({"cluster", "info"}).GetString();
  EXPECT_THAT(cluster_info, HasSubstr("cluster_state:ok"));
  EXPECT_THAT(cluster_info, HasSubstr("cluster_slots_assigned:16384"));
  EXPECT_THAT(cluster_info, HasSubstr("cluster_slots_ok:16384"));
  EXPECT_THAT(cluster_info, HasSubstr("cluster_known_nodes:1"));
  EXPECT_THAT(cluster_info, HasSubstr("cluster_size:1"));
  // TODO: Use "CLUSTER SLOTS" and "CLUSTER SHARDS" once implemented to verify new configuration
  // takes effect.
}

TEST_F(ClusterFamilyTest, ClusterConfigFull) {
  EXPECT_EQ(Run({"dflycluster", "config", R"json(
      [
        {
          "slot_ranges": [
            {
              "start": 0,
              "end": 16383
            }
          ],
          "master": {
            "id": "abcd1234",
            "ip": "10.0.0.1",
            "port": 7000
          },
          "replicas": [
            {
              "id": "wxyz",
              "ip": "10.0.0.10",
              "port": 8000
            }
          ]
        }
      ])json"}),
            "OK");

  string cluster_info = Run({"cluster", "info"}).GetString();
  EXPECT_THAT(cluster_info, HasSubstr("cluster_state:ok"));
  EXPECT_THAT(cluster_info, HasSubstr("cluster_slots_assigned:16384"));
  EXPECT_THAT(cluster_info, HasSubstr("cluster_slots_ok:16384"));
  EXPECT_THAT(cluster_info, HasSubstr("cluster_known_nodes:2"));
  EXPECT_THAT(cluster_info, HasSubstr("cluster_size:1"));
  // TODO: Use "CLUSTER SLOTS" and "CLUSTER SHARDS" once implemented to verify new configuration
  // takes effect.
}

TEST_F(ClusterFamilyTest, ClusterConfigFullMultipleInstances) {
  EXPECT_EQ(Run({"dflycluster", "config", R"json(
      [
        {
          "slot_ranges": [
            {
              "start": 0,
              "end": 10000
            }
          ],
          "master": {
            "id": "abcd1234",
            "ip": "10.0.0.1",
            "port": 7000
          },
          "replicas": [
            {
              "id": "wxyz",
              "ip": "10.0.0.10",
              "port": 8000
            }
          ]
        },
        {
          "slot_ranges": [
            {
              "start": 10001,
              "end": 16383
            }
          ],
          "master": {
            "id": "efgh7890",
            "ip": "10.0.0.2",
            "port": 7001
          },
          "replicas": [
            {
              "id": "qwerty",
              "ip": "10.0.0.11",
              "port": 8001
            }
          ]
        }
      ])json"}),
            "OK");

  string cluster_info = Run({"cluster", "info"}).GetString();
  EXPECT_THAT(cluster_info, HasSubstr("cluster_state:ok"));
  EXPECT_THAT(cluster_info, HasSubstr("cluster_slots_assigned:16384"));
  EXPECT_THAT(cluster_info, HasSubstr("cluster_slots_ok:16384"));
  EXPECT_THAT(cluster_info, HasSubstr("cluster_known_nodes:4"));
  EXPECT_THAT(cluster_info, HasSubstr("cluster_size:2"));
  // TODO: Use "CLUSTER SLOTS" and "CLUSTER SHARDS" once implemented to verify new configuration
  // takes effect.
}

TEST_F(ClusterFamilyTest, ClusterGetSlotInfo) {
  string node_id = Run({"dflycluster", "myid"}).GetString();
  string config_prefix = R"json(
      [
        {
          "slot_ranges": [
            {
              "start": 0,
              "end": 16383
            }
          ],
          "master": {
            "id": ")json";
  string config_suffix = R"json(",
            "ip": "10.0.0.1",
            "port": 7000
          },
          "replicas": []
        }
      ])json";
  string full_config = absl::StrCat(config_prefix, node_id, config_suffix);

  EXPECT_EQ(Run({"dflycluster", "config", full_config}), "OK");

  Run({"debug", "populate", "100000"});

  auto slots_info = Run({"dflycluster", "getslotinfo", "slots", "1", "2"}).GetVec();
  EXPECT_EQ(slots_info.size(), 2);
  auto slot1 = slots_info[0].GetVec();
  EXPECT_EQ(slot1.size(), 3);
  EXPECT_EQ(slot1[0], "1");
  EXPECT_EQ(slot1[1], "key_count");
  EXPECT_NE(slot1[2], "0");
  auto slot2 = slots_info[1].GetVec();
  EXPECT_EQ(slot2.size(), 3);
  EXPECT_EQ(slot2[0], "2");
  EXPECT_EQ(slot2[1], "key_count");
  EXPECT_NE(slot2[2], "0");
}

TEST_F(ClusterFamilyTest, ClusterConfigDeleteSlots) {
  string node_id = Run({"dflycluster", "myid"}).GetString();
  string config_prefix = R"json(
      [
        {
          "slot_ranges": [
            {
              "start": 0,
              "end": 16383
            }
          ],
          "master": {
            "id": ")json";
  string config_suffix = R"json(",
            "ip": "10.0.0.1",
            "port": 7000
          },
          "replicas": []
        }
      ])json";
  string full_config = absl::StrCat(config_prefix, node_id, config_suffix);

  EXPECT_EQ(Run({"dflycluster", "config", full_config}), "OK");

  Run({"debug", "populate", "100000"});

  auto slots_info = Run({"dflycluster", "getslotinfo", "slots", "1", "2"}).GetVec();
  EXPECT_EQ(slots_info.size(), 2);
  auto slot1 = slots_info[0].GetVec();
  EXPECT_EQ(slot1.size(), 3);
  EXPECT_EQ(slot1[0], "1");
  EXPECT_EQ(slot1[1], "key_count");
  EXPECT_NE(slot1[2], "0");
  auto slot2 = slots_info[1].GetVec();
  EXPECT_EQ(slot2.size(), 3);
  EXPECT_EQ(slot2[0], "2");
  EXPECT_EQ(slot2[1], "key_count");
  EXPECT_NE(slot2[2], "0");

  string new_config = absl::StrCat(config_prefix, "abc", config_suffix);
  EXPECT_EQ(Run({"dflycluster", "config", new_config}), "OK");
  sleep(1);
  slots_info = Run({"dflycluster", "getslotinfo", "slots", "1", "2"}).GetVec();
  EXPECT_EQ(slots_info.size(), 2);
  slot1 = slots_info[0].GetVec();
  EXPECT_EQ(slot1.size(), 3);
  EXPECT_EQ(slot1[0], "1");
  EXPECT_EQ(slot1[1], "key_count");
  EXPECT_EQ(slot1[2], "0");
  slot2 = slots_info[1].GetVec();
  EXPECT_EQ(slot2.size(), 3);
  EXPECT_EQ(slot2[0], "2");
  EXPECT_EQ(slot2[1], "key_count");
  EXPECT_EQ(slot2[2], "0");
}

class ClusterFamilyEmulatedTest : public BaseFamilyTest {
 public:
  ClusterFamilyEmulatedTest() {
    auto* flag = absl::FindCommandLineFlag("cluster_mode");
    CHECK_NE(flag, nullptr);
    string error;
    CHECK(flag->ParseFrom("emulated", &error));
  }
};

TEST_F(ClusterFamilyEmulatedTest, ClusterInfo) {
  string cluster_info = Run({"cluster", "info"}).GetString();
  EXPECT_THAT(cluster_info, HasSubstr("cluster_state:ok"));
  EXPECT_THAT(cluster_info, HasSubstr("cluster_slots_assigned:16384"));
  EXPECT_THAT(cluster_info, HasSubstr("cluster_slots_ok:16384"));
  EXPECT_THAT(cluster_info, HasSubstr("cluster_known_nodes:1"));
  EXPECT_THAT(cluster_info, HasSubstr("cluster_size:1"));
}

}  // namespace
}  // namespace dfly
