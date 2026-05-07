#pragma once

namespace meshtext {
enum class EditPagesAPStartResult {
    Started,
    Rebooting,
    Failed,
};

EditPagesAPStartResult startEditPagesAP();
bool startEditPagesAPIfRequested();
bool isEditPagesAPActive();
}
