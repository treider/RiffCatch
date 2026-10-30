#pragma once
#include <juce_data_structures/juce_data_structures.h>
#include <functional>

// Undoable action defined by two lambdas. `redo` is also executed when the action is first performed,
// so it must be safe to run when the change has already been applied.
struct LambdaAction : juce::UndoableAction
{
    LambdaAction (std::function<void()> r, std::function<void()> u) : redoFn (std::move (r)), undoFn (std::move (u)) {}
    bool perform() override { redoFn(); return true; }
    bool undo() override    { undoFn(); return true; }
    std::function<void()> redoFn, undoFn;
};

using UndoPush = std::function<void (const juce::String& name, std::function<void()> redo, std::function<void()> undo)>;
