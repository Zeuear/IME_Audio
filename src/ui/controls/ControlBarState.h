#pragma once
#include <QString>
#include "services/WorkflowManager.h"

namespace ControlBarState {

struct Buttons {
    bool startEnabled = false;
    bool stopEnabled = false;
};

Buttons forWorkflow(WorkflowState state);
bool isSessionActive(WorkflowState state);
QString formatElapsed(qint64 seconds);

}
