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

bool isSessionActive(WorkflowState state)
{
    return state != WorkflowState::Idle && state != WorkflowState::Error;
}

QString formatElapsed(qint64 seconds)
{
    if (seconds < 0) seconds = 0;
    const qint64 h = seconds / 3600, m = seconds / 60 % 60, s = seconds % 60;
    if (h > 0) return QStringLiteral("%1:%2:%3").arg(h).arg(m, 2, 10, QLatin1Char('0')).arg(s, 2, 10, QLatin1Char('0'));
    return QStringLiteral("%1:%2").arg(m, 2, 10, QLatin1Char('0')).arg(s, 2, 10, QLatin1Char('0'));
}

} // namespace ControlBarState
