#include "dialog_policy.h"
#include "OynonRuntimeApi.h"
#include <cstring>

namespace disco_dialogues {
namespace {
BOOL __stdcall Execute(const OynonUIExecuteCall* call, void*) {
    if (!call->name || std::strcmp(call->name, "DiscoDialoguesPrint") != 0)
        return OynonProceedUIExecute(call, call->name, call->count, nullptr);
    if (call->count != 11) return FALSE;
    OynonVerticalViewport viewport{};
    if (!OynonUIReadInt(call, 9, &viewport.top) || !OynonUIReadInt(call, 10, &viewport.height))
        return FALSE;
    return OynonProceedUIExecute(call, "PrintInWidth", 9, &viewport);
}
}
bool InstallDialogFeed() { return OynonInstallUIExecuteHook(Execute, nullptr) != FALSE; }
}
