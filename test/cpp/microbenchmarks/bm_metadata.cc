/*
 *
 * Copyright 2017, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

/* Test out various metadata handling primitives */

#include <grpc/grpc.h>

extern "C" {
#include "src/core/lib/slice/slice_internal.h"
#include "src/core/lib/transport/metadata.h"
#include "src/core/lib/transport/static_metadata.h"
#include "src/core/lib/transport/transport.h"
}

#include "test/cpp/microbenchmarks/helpers.h"
#include "third_party/benchmark/include/benchmark/benchmark.h"

auto& force_library_initialization = Library::get();

static void BM_SliceFromStatic(benchmark::State& state) {
  TrackCounters track_counters;
  while (state.KeepRunning()) {
    benchmark::DoNotOptimize(grpc_slice_from_static_string("abc"));
  }
  track_counters.Finish(state);
}
BENCHMARK(BM_SliceFromStatic);

static void BM_SliceFromCopied(benchmark::State& state) {
  TrackCounters track_counters;
  while (state.KeepRunning()) {
    grpc_slice_unref(grpc_slice_from_copied_string("abc"));
  }
  track_counters.Finish(state);
}
BENCHMARK(BM_SliceFromCopied);

static void BM_SliceFromStreamOwnedBuffer(benchmark::State& state) {
  grpc_stream_refcount r;
  GRPC_STREAM_REF_INIT(&r, 1, NULL, NULL, "test");
  char buffer[64];
  grpc_exec_ctx exec_ctx = GRPC_EXEC_CTX_INIT;
  while (state.KeepRunning()) {
    grpc_slice_unref_internal(&exec_ctx, grpc_slice_from_stream_owned_buffer(
                                             &r, buffer, sizeof(buffer)));
  }
  grpc_exec_ctx_finish(&exec_ctx);
}
BENCHMARK(BM_SliceFromStreamOwnedBuffer);

static void BM_SliceIntern(benchmark::State& state) {
  TrackCounters track_counters;
  gpr_slice slice = grpc_slice_from_static_string("abc");
  while (state.KeepRunning()) {
    grpc_slice_unref(grpc_slice_intern(slice));
  }
  track_counters.Finish(state);
}
BENCHMARK(BM_SliceIntern);

static void BM_SliceReIntern(benchmark::State& state) {
  TrackCounters track_counters;
  gpr_slice slice = grpc_slice_intern(grpc_slice_from_static_string("abc"));
  while (state.KeepRunning()) {
    grpc_slice_unref(grpc_slice_intern(slice));
  }
  grpc_slice_unref(slice);
  track_counters.Finish(state);
}
BENCHMARK(BM_SliceReIntern);

static void BM_SliceInternStaticMetadata(benchmark::State& state) {
  TrackCounters track_counters;
  while (state.KeepRunning()) {
    grpc_slice_intern(GRPC_MDSTR_GZIP);
  }
  track_counters.Finish(state);
}
BENCHMARK(BM_SliceInternStaticMetadata);

static void BM_SliceInternEqualToStaticMetadata(benchmark::State& state) {
  TrackCounters track_counters;
  gpr_slice slice = grpc_slice_from_static_string("gzip");
  while (state.KeepRunning()) {
    grpc_slice_intern(slice);
  }
  track_counters.Finish(state);
}
BENCHMARK(BM_SliceInternEqualToStaticMetadata);

static void BM_MetadataFromNonInternedSlices(benchmark::State& state) {
  TrackCounters track_counters;
  gpr_slice k = grpc_slice_from_static_string("key");
  gpr_slice v = grpc_slice_from_static_string("value");
  grpc_exec_ctx exec_ctx = GRPC_EXEC_CTX_INIT;
  while (state.KeepRunning()) {
    GRPC_MDELEM_UNREF(&exec_ctx, grpc_mdelem_create(&exec_ctx, k, v, NULL));
  }
  grpc_exec_ctx_finish(&exec_ctx);
  track_counters.Finish(state);
}
BENCHMARK(BM_MetadataFromNonInternedSlices);

static void BM_MetadataFromInternedSlices(benchmark::State& state) {
  TrackCounters track_counters;
  gpr_slice k = grpc_slice_intern(grpc_slice_from_static_string("key"));
  gpr_slice v = grpc_slice_intern(grpc_slice_from_static_string("value"));
  grpc_exec_ctx exec_ctx = GRPC_EXEC_CTX_INIT;
  while (state.KeepRunning()) {
    GRPC_MDELEM_UNREF(&exec_ctx, grpc_mdelem_create(&exec_ctx, k, v, NULL));
  }
  grpc_exec_ctx_finish(&exec_ctx);
  grpc_slice_unref(k);
  grpc_slice_unref(v);
  track_counters.Finish(state);
}
BENCHMARK(BM_MetadataFromInternedSlices);

static void BM_MetadataFromInternedSlicesAlreadyInIndex(
    benchmark::State& state) {
  TrackCounters track_counters;
  gpr_slice k = grpc_slice_intern(grpc_slice_from_static_string("key"));
  gpr_slice v = grpc_slice_intern(grpc_slice_from_static_string("value"));
  grpc_exec_ctx exec_ctx = GRPC_EXEC_CTX_INIT;
  grpc_mdelem seed = grpc_mdelem_create(&exec_ctx, k, v, NULL);
  while (state.KeepRunning()) {
    GRPC_MDELEM_UNREF(&exec_ctx, grpc_mdelem_create(&exec_ctx, k, v, NULL));
  }
  GRPC_MDELEM_UNREF(&exec_ctx, seed);
  grpc_exec_ctx_finish(&exec_ctx);
  grpc_slice_unref(k);
  grpc_slice_unref(v);
  track_counters.Finish(state);
}
BENCHMARK(BM_MetadataFromInternedSlicesAlreadyInIndex);

static void BM_MetadataFromInternedKey(benchmark::State& state) {
  TrackCounters track_counters;
  gpr_slice k = grpc_slice_intern(grpc_slice_from_static_string("key"));
  gpr_slice v = grpc_slice_from_static_string("value");
  grpc_exec_ctx exec_ctx = GRPC_EXEC_CTX_INIT;
  while (state.KeepRunning()) {
    GRPC_MDELEM_UNREF(&exec_ctx, grpc_mdelem_create(&exec_ctx, k, v, NULL));
  }
  grpc_exec_ctx_finish(&exec_ctx);
  grpc_slice_unref(k);
  track_counters.Finish(state);
}
BENCHMARK(BM_MetadataFromInternedKey);

static void BM_MetadataFromNonInternedSlicesWithBackingStore(
    benchmark::State& state) {
  TrackCounters track_counters;
  gpr_slice k = grpc_slice_from_static_string("key");
  gpr_slice v = grpc_slice_from_static_string("value");
  char backing_store[sizeof(grpc_mdelem_data)];
  grpc_exec_ctx exec_ctx = GRPC_EXEC_CTX_INIT;
  while (state.KeepRunning()) {
    GRPC_MDELEM_UNREF(
        &exec_ctx,
        grpc_mdelem_create(&exec_ctx, k, v,
                           reinterpret_cast<grpc_mdelem_data*>(backing_store)));
  }
  grpc_exec_ctx_finish(&exec_ctx);
  track_counters.Finish(state);
}
BENCHMARK(BM_MetadataFromNonInternedSlicesWithBackingStore);

static void BM_MetadataFromInternedSlicesWithBackingStore(
    benchmark::State& state) {
  TrackCounters track_counters;
  gpr_slice k = grpc_slice_intern(grpc_slice_from_static_string("key"));
  gpr_slice v = grpc_slice_intern(grpc_slice_from_static_string("value"));
  char backing_store[sizeof(grpc_mdelem_data)];
  grpc_exec_ctx exec_ctx = GRPC_EXEC_CTX_INIT;
  while (state.KeepRunning()) {
    GRPC_MDELEM_UNREF(
        &exec_ctx,
        grpc_mdelem_create(&exec_ctx, k, v,
                           reinterpret_cast<grpc_mdelem_data*>(backing_store)));
  }
  grpc_exec_ctx_finish(&exec_ctx);
  grpc_slice_unref(k);
  grpc_slice_unref(v);
  track_counters.Finish(state);
}
BENCHMARK(BM_MetadataFromInternedSlicesWithBackingStore);

static void BM_MetadataFromInternedKeyWithBackingStore(
    benchmark::State& state) {
  TrackCounters track_counters;
  gpr_slice k = grpc_slice_intern(grpc_slice_from_static_string("key"));
  gpr_slice v = grpc_slice_from_static_string("value");
  char backing_store[sizeof(grpc_mdelem_data)];
  grpc_exec_ctx exec_ctx = GRPC_EXEC_CTX_INIT;
  while (state.KeepRunning()) {
    GRPC_MDELEM_UNREF(
        &exec_ctx,
        grpc_mdelem_create(&exec_ctx, k, v,
                           reinterpret_cast<grpc_mdelem_data*>(backing_store)));
  }
  grpc_exec_ctx_finish(&exec_ctx);
  grpc_slice_unref(k);
  track_counters.Finish(state);
}
BENCHMARK(BM_MetadataFromInternedKeyWithBackingStore);

static void BM_MetadataFromStaticMetadataStrings(benchmark::State& state) {
  TrackCounters track_counters;
  gpr_slice k = GRPC_MDSTR_STATUS;
  gpr_slice v = GRPC_MDSTR_200;
  grpc_exec_ctx exec_ctx = GRPC_EXEC_CTX_INIT;
  while (state.KeepRunning()) {
    GRPC_MDELEM_UNREF(&exec_ctx, grpc_mdelem_create(&exec_ctx, k, v, NULL));
  }
  grpc_exec_ctx_finish(&exec_ctx);
  grpc_slice_unref(k);
  track_counters.Finish(state);
}
BENCHMARK(BM_MetadataFromStaticMetadataStrings);

static void BM_MetadataFromStaticMetadataStringsNotIndexed(
    benchmark::State& state) {
  TrackCounters track_counters;
  gpr_slice k = GRPC_MDSTR_STATUS;
  gpr_slice v = GRPC_MDSTR_GZIP;
  grpc_exec_ctx exec_ctx = GRPC_EXEC_CTX_INIT;
  while (state.KeepRunning()) {
    GRPC_MDELEM_UNREF(&exec_ctx, grpc_mdelem_create(&exec_ctx, k, v, NULL));
  }
  grpc_exec_ctx_finish(&exec_ctx);
  grpc_slice_unref(k);
  track_counters.Finish(state);
}
BENCHMARK(BM_MetadataFromStaticMetadataStringsNotIndexed);

static void BM_MetadataRefUnrefExternal(benchmark::State& state) {
  TrackCounters track_counters;
  char backing_store[sizeof(grpc_mdelem_data)];
  grpc_exec_ctx exec_ctx = GRPC_EXEC_CTX_INIT;
  grpc_mdelem el =
      grpc_mdelem_create(&exec_ctx, grpc_slice_from_static_string("a"),
                         grpc_slice_from_static_string("b"),
                         reinterpret_cast<grpc_mdelem_data*>(backing_store));
  while (state.KeepRunning()) {
    GRPC_MDELEM_UNREF(&exec_ctx, GRPC_MDELEM_REF(el));
  }
  GRPC_MDELEM_UNREF(&exec_ctx, el);
  grpc_exec_ctx_finish(&exec_ctx);
  track_counters.Finish(state);
}
BENCHMARK(BM_MetadataRefUnrefExternal);

static void BM_MetadataRefUnrefInterned(benchmark::State& state) {
  TrackCounters track_counters;
  char backing_store[sizeof(grpc_mdelem_data)];
  grpc_exec_ctx exec_ctx = GRPC_EXEC_CTX_INIT;
  gpr_slice k = grpc_slice_intern(grpc_slice_from_static_string("key"));
  gpr_slice v = grpc_slice_intern(grpc_slice_from_static_string("value"));
  grpc_mdelem el = grpc_mdelem_create(
      &exec_ctx, k, v, reinterpret_cast<grpc_mdelem_data*>(backing_store));
  grpc_slice_unref(k);
  grpc_slice_unref(v);
  while (state.KeepRunning()) {
    GRPC_MDELEM_UNREF(&exec_ctx, GRPC_MDELEM_REF(el));
  }
  GRPC_MDELEM_UNREF(&exec_ctx, el);
  grpc_exec_ctx_finish(&exec_ctx);
  track_counters.Finish(state);
}
BENCHMARK(BM_MetadataRefUnrefInterned);

static void BM_MetadataRefUnrefAllocated(benchmark::State& state) {
  TrackCounters track_counters;
  grpc_exec_ctx exec_ctx = GRPC_EXEC_CTX_INIT;
  grpc_mdelem el =
      grpc_mdelem_create(&exec_ctx, grpc_slice_from_static_string("a"),
                         grpc_slice_from_static_string("b"), NULL);
  while (state.KeepRunning()) {
    GRPC_MDELEM_UNREF(&exec_ctx, GRPC_MDELEM_REF(el));
  }
  GRPC_MDELEM_UNREF(&exec_ctx, el);
  grpc_exec_ctx_finish(&exec_ctx);
  track_counters.Finish(state);
}
BENCHMARK(BM_MetadataRefUnrefAllocated);

static void BM_MetadataRefUnrefStatic(benchmark::State& state) {
  TrackCounters track_counters;
  grpc_exec_ctx exec_ctx = GRPC_EXEC_CTX_INIT;
  grpc_mdelem el =
      grpc_mdelem_create(&exec_ctx, GRPC_MDSTR_STATUS, GRPC_MDSTR_200, NULL);
  while (state.KeepRunning()) {
    GRPC_MDELEM_UNREF(&exec_ctx, GRPC_MDELEM_REF(el));
  }
  GRPC_MDELEM_UNREF(&exec_ctx, el);
  grpc_exec_ctx_finish(&exec_ctx);
  track_counters.Finish(state);
}
BENCHMARK(BM_MetadataRefUnrefStatic);

BENCHMARK_MAIN();
