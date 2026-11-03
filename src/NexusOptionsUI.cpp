#include "imgui180/prelude.h"

#include <NexusOptionsUI.h>

namespace imgui180
{
#include "../extern/imgui-nexus/imgui.h"
}

namespace NexusOptionsUI
{

void Init(void* imguiContext, void* imguiMalloc, void* imguiFree)
{
    using namespace imgui180;

    ImGui::SetCurrentContext(static_cast<ImGuiContext*>(imguiContext));
    ImGui::SetAllocatorFunctions(reinterpret_cast<void* (*)(size_t, void*)>(imguiMalloc), reinterpret_cast<void (*)(void*, void*)>(imguiFree));
}

bool Draw()
{
    using namespace imgui180;

    return ImGui::Button("Open GW2Radial settings##GW2Radial");
}

} // namespace NexusOptionsUI
