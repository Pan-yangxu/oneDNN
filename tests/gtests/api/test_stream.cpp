/*******************************************************************************
* Copyright 2019 Intel Corporation
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*******************************************************************************/

#include "mkldnn_test_common.hpp"
#include "gtest/gtest.h"

#include "mkldnn.h"

#include <tuple>

namespace mkldnn {

static bool are_valid_flags(mkldnn_engine_kind_t engine_kind,
        mkldnn_stream_flags_t stream_flags) {
    bool ok = true;
#if MKLDNN_GPU_RUNTIME == MKLDNN_RUNTIME_OCL
    if (engine_kind == mkldnn_gpu
            && (stream_flags & mkldnn_stream_out_of_order))
        ok = false;
#endif
#if MKLDNN_CPU_RUNTIME != MKLDNN_RUNTIME_SYCL
    if (engine_kind == mkldnn_cpu
            && (stream_flags & mkldnn_stream_out_of_order))
        ok = false;
#endif
    return ok;
}

class stream_test_c
    : public ::testing::TestWithParam<
              std::tuple<mkldnn_engine_kind_t, mkldnn_stream_flags_t>>
{
protected:
    void SetUp() override {
        std::tie(eng_kind, stream_flags) = GetParam();

        if (mkldnn_engine_get_count(eng_kind) == 0)
            return;

        MKLDNN_CHECK(mkldnn_engine_create(&engine, eng_kind, 0));

        // Check that the flags are compatible with the engine
        if (!are_valid_flags(eng_kind, stream_flags)) {
            MKLDNN_CHECK(mkldnn_engine_destroy(engine));
            engine = nullptr;
            return;
        }

        MKLDNN_CHECK(mkldnn_stream_create(&stream, engine, stream_flags));
    }

    void TearDown() override {
        if (stream) {
            MKLDNN_CHECK(mkldnn_stream_destroy(stream));
        }
        if (engine) {
            MKLDNN_CHECK(mkldnn_engine_destroy(engine));
        }
    }

    mkldnn_engine_kind_t eng_kind;
    mkldnn_stream_flags_t stream_flags;

    mkldnn_engine_t engine = nullptr;
    mkldnn_stream_t stream = nullptr;
};

class stream_test_cpp
    : public ::testing::TestWithParam<
              std::tuple<mkldnn_engine_kind_t, mkldnn_stream_flags_t>>
{
};

TEST_P(stream_test_c, Create) {
    SKIP_IF(!engine, "Engines not found or stream flags are incompatible.");

    MKLDNN_CHECK(mkldnn_stream_wait(stream));
}

TEST(stream_test_c, WaitNullStream) {
    mkldnn_stream_t stream = nullptr;
    mkldnn_status_t status = mkldnn_stream_wait(stream);
    ASSERT_EQ(status, mkldnn_invalid_arguments);
}

TEST_P(stream_test_c, Wait) {
    SKIP_IF(!engine, "Engines not found or stream flags are incompatible.");

    MKLDNN_CHECK(mkldnn_stream_wait(stream));
}

TEST_P(stream_test_cpp, Wait) {
    mkldnn_engine_kind_t eng_kind_c;
    mkldnn_stream_flags_t stream_flags_c;
    std::tie(eng_kind_c, stream_flags_c) = GetParam();

    engine::kind eng_kind = static_cast<engine::kind>(eng_kind_c);
    stream::flags stream_flags = static_cast<stream::flags>(stream_flags_c);
    SKIP_IF(engine::get_count(eng_kind) == 0, "Engines not found.");

    engine eng(eng_kind, 0);
    SKIP_IF(!are_valid_flags(
                    static_cast<mkldnn_engine_kind_t>(eng.get_kind()),
                    stream_flags_c),
            "Incompatible stream flags.");

    stream s(eng, stream_flags);
    s.wait();
}

namespace {
struct PrintToStringParamName {
    template <class ParamType>
    std::string operator()(
            const ::testing::TestParamInfo<ParamType> &info) const {
        return to_string(std::get<0>(info.param)) + "_"
                + to_string(std::get<1>(info.param));
    }
};

auto all_params = ::testing::Combine(::testing::Values(mkldnn_cpu, mkldnn_gpu),
        ::testing::Values(mkldnn_stream_default_flags, mkldnn_stream_in_order,
                mkldnn_stream_out_of_order));

} // namespace

INSTANTIATE_TEST_SUITE_P(
        AllEngineKinds, stream_test_c, all_params, PrintToStringParamName());
INSTANTIATE_TEST_SUITE_P(
        AllEngineKinds, stream_test_cpp, all_params, PrintToStringParamName());

} // namespace mkldnn