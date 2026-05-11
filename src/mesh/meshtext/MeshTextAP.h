#pragma once

namespace meshtext {
enum class EditPagesAPStartResult {
    Started,
    Rebooting,
    Failed,
};

EditPagesAPStartResult startEditPagesAP();
bool startEditPagesAPIfRequested();
void stopEditPagesAP();
bool isEditPagesAPActive();
const char *getEditPagesAPSSID();
}
