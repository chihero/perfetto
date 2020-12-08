/*
 * Copyright (C) 2020 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "src/trace_processor/importers/proto/async_track_set_tracker.h"

#include "src/trace_processor/importers/common/track_tracker.h"
#include "src/trace_processor/types/trace_processor_context.h"

namespace perfetto {
namespace trace_processor {

AsyncTrackSetTracker::AsyncTrackSetTracker(TraceProcessorContext* context)
    : context_(context) {}

AsyncTrackSetTracker::TrackSetId AsyncTrackSetTracker::InternAndroidSet(
    UniquePid upid,
    StringId name) {
  AndroidTuple tuple{upid, name};

  auto it = android_track_set_ids_.find(tuple);
  if (it != android_track_set_ids_.end())
    return it->second;

  uint32_t id = static_cast<uint32_t>(track_sets_.size());

  TrackSet set;
  set.android_tuple = tuple;
  set.type = TrackSetType::kAndroid;
  track_sets_.emplace_back(set);
  android_track_set_ids_[tuple] = id;
  return id;
}

TrackId AsyncTrackSetTracker::Begin(TrackSetId id,
                                    int64_t cookie,
                                    NestingBehaviour nest) {
  PERFETTO_DCHECK(id < track_sets_.size());

  TrackSet& set = track_sets_[id];

  auto it = std::find_if(
      set.tracks.begin(), set.tracks.end(),
      [cookie](const TrackState& state) { return state.cookie == cookie; });
  TrackState& state = it == set.tracks.end()
                          ? set.tracks[GetOrCreateEmptyTrack(set, cookie)]
                          : *it;
  PERFETTO_DCHECK(it != set.tracks.end() || state.nest_count == 0);

  switch (nest) {
    case NestingBehaviour::kLegacySaturatingUnnestable:
      PERFETTO_DCHECK(state.nest_count <= 1);
      state.nest_count = 1;
      break;
    case NestingBehaviour::kUnnestable:
      PERFETTO_DCHECK(state.nest_count == 0);
      state.nest_count++;
      break;
  }
  return state.id;
}

TrackId AsyncTrackSetTracker::End(TrackSetId id, int64_t cookie) {
  PERFETTO_DCHECK(id < track_sets_.size());

  TrackSet& set = track_sets_[id];
  auto it = std::find_if(
      set.tracks.begin(), set.tracks.end(),
      [cookie](const TrackState& state) { return state.cookie == cookie; });
  if (it == set.tracks.end())
    return set.tracks[GetOrCreateEmptyTrack(set, cookie)].id;

  // It's possible to have a nest count of 0 even when we know about the track.
  // Suppose the following sequence of events for some |id| and |cookie|:
  //   Begin
  //   (trace starts)
  //   Begin
  //   End
  //   End <- nest count == 0 here even though we have a record of this track.
  if (it->nest_count > 0)
    it->nest_count--;
  return it->id;
}

uint32_t AsyncTrackSetTracker::GetOrCreateEmptyTrack(TrackSet& set,
                                                     int64_t cookie) {
  auto it = std::find_if(
      set.tracks.begin(), set.tracks.end(),
      [](const TrackState& state) { return state.nest_count == 0; });
  if (it != set.tracks.end())
    return static_cast<uint32_t>(std::distance(set.tracks.begin(), it));

  TrackState state;
  state.cookie = cookie;
  state.nest_count = 0;
  switch (set.type) {
    case TrackSetType::kAndroid:
      state.id = context_->track_tracker->CreateAndroidAsyncTrack(
          set.android_tuple.name, set.android_tuple.upid);
      break;
  }

  uint32_t idx = static_cast<uint32_t>(set.tracks.size());
  set.tracks.emplace_back(state);
  return idx;
}

}  // namespace trace_processor
}  // namespace perfetto
