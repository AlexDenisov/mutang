#include "mull/Reporters/IDEReporter.h"

#include "mull/Logger.h"
#include "mull/MutationResult.h"
#include "mull/Mutators/Mutator.h"
#include "mull/Reporters/SourceManager.h"
#include "mull/Result.h"

#include <map>
#include <set>
#include <utility>

using namespace mull;

static bool mutantSurvived(const ExecutionStatus &status) {
  return status == ExecutionStatus::Passed;
}

static void printMutant(SourceManager &sourceManager, const MutationPoint &mutant, bool survived) {
  auto &sourceLocation = mutant.getSourceLocation();
  assert(!sourceLocation.isNull() && "Debug information is missing?");

  const std::string status = survived ? "Survived" : "Killed";
  Logger::info() << sourceLocation.filePath << ":" << sourceLocation.line << ":"
                 << sourceLocation.column << ": warning: " << status << ": "
                 << mutant.getDiagnostics() << " "
                 << "[" << mutant.getMutatorIdentifier() << "]"
                 << "\n";

  auto line = sourceManager.getLine(sourceLocation);
  assert(sourceLocation.column < line.size());

  std::string caret(sourceLocation.column, ' ');
  for (size_t index = 0; index < sourceLocation.column; index++) {
    if (line[index] == '\t') {
      caret[index] = '\t';
    }
  }
  caret[sourceLocation.column - 1] = '^';

  Logger::info() << line;
  Logger::info() << caret << "\n";
}

IDEReporter::IDEReporter(bool showKilled) : showKilled(showKilled) {}

void IDEReporter::reportResults(const Result &result, const Metrics &metrics) {
  if (result.getMutationPoints().empty()) {
    Logger::info() << "No mutants found. Mutation score: infinitely high\n";
    return;
  }

  std::set<MutationPoint *> killedMutants;
  for (auto &mutationResult : result.getMutationResults()) {
    auto mutant = mutationResult->getMutationPoint();
    auto &executionResult = mutationResult->getExecutionResult();

    if (!mutantSurvived(executionResult.status)) {
      killedMutants.insert(mutant);
    }
  }

  auto survivedMutantsCount = result.getMutationPoints().size() - killedMutants.size();
  auto killedMutantsCount = killedMutants.size();

  SourceManager sourceManager;

  /// It is important that below we iterate over result.getMutationPoints()
  /// because we want the output to be stable across multiple executions of Mull.
  /// Optimizing this here was a bad idea: https://github.com/mull-project/mull/pull/640.
  if (showKilled && killedMutantsCount > 0) {
    Logger::info() << "\nKilled mutants (" << killedMutantsCount << "/"
                   << result.getMutationPoints().size() << "):\n\n";

    for (auto &mutant : result.getMutationPoints()) {
      if (killedMutants.find(mutant) != killedMutants.end()) {
        printMutant(sourceManager, *mutant, false);
      }
    }
  }

  if (survivedMutantsCount > 0) {
    Logger::info() << "\nSurvived mutants (" << survivedMutantsCount << "/"
                   << result.getMutationPoints().size() << "):\n\n";

    for (auto &mutant : result.getMutationPoints()) {
      if (killedMutants.find(mutant) == killedMutants.end()) {
        printMutant(sourceManager, *mutant, true);
      }
    }
    Logger::info() << "\n";
  } else {
    Logger::info() << "\nAll mutations have been killed\n\n";
  }

  auto rawScore = double(killedMutants.size()) / double(result.getMutationPoints().size());
  auto score = uint(rawScore * 100);
  Logger::info() << "Mutation score: " << score << "%\n";
}
