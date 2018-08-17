#include "MutationsFinder.h"
#include "Testee.h"

#include "Parallelization/Parallelization.h"

using namespace mull;
using namespace llvm;

MutationsFinder::MutationsFinder(std::vector<std::unique_ptr<Mutator>> mutators)
: mutators(std::move(mutators)) {}

std::vector<MutationPoint *> MutationsFinder::getMutationPoints(const Context &context,
                                                                std::vector<MergedTestee> &testees,
                                                                Filter &filter) {
  int workers = std::thread::hardware_concurrency();
  std::vector<SearchMutationPointsTask> tasks;
  for (int i = 0; i < workers; i++) {
    tasks.emplace_back(filter, context, mutators);
  }

  TaskExecutor<SearchMutationPointsTask> finder("Searching mutants across functions", testees, ownedPoints, tasks);
  finder.execute();

  std::vector<MutationPoint *> mutationPoints;
  for (auto &point : ownedPoints) {
    mutationPoints.push_back(point.get());
  }

  return mutationPoints;
}
