#include "ControlBarState.h"

namespace ControlBarState {

Buttons forWorkflow(WorkflowState state)
{
    switch (state) {
    case WorkflowState::Idle:
        return { true, false };
    case WorkflowState::Loading:
    case WorkflowState::Recording:
    case WorkflowState::Transcribing:
    case WorkflowState::Processing:
        return { false, true };
    case WorkflowState::Stopping:
    case WorkflowState::Error:
        break;
    }
    return { false, false };
}

} // namespace ControlBarState
