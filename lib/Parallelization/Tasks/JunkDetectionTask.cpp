#include "Parallelization/Tasks/JunkDetectionTask.h"
#include "Parallelization/Progress.h"
#include "JunkDetection/JunkDetector.h"

using namespace mull;

JunkDetectionTask::JunkDetectionTask(JunkDetector &detector)
  : detector(detector) {}

void JunkDetectionTask::operator()(iterator begin, iterator end,
                                   Out &storage, progress_counter &counter) {
  for (auto it = begin; it != end; ++it, counter.increment()) {
    auto point = *it;
    if (detector.isJunk(point)) {
      continue;
    }
    storage.push_back(point);
  }
}


