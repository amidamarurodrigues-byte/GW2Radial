#pragma once

// The contents of the addon's entry in the Nexus options panel. Nexus draws it with its own ImGui (1.80), so this lives in a
// separate translation unit that talks to that version and exposes no ImGui types.
namespace NexusOptionsUI
{

// Hooks the ImGui copy used here up to the context and allocators Nexus hands to addons.
void Init(void* imguiContext, void* imguiMalloc, void* imguiFree);

struct Actions
{
    bool hideIconChanged = false;
    bool openSettings    = false;
};

// Call from Nexus' options render callback.
Actions Draw(bool& hideQuickAccessIcon);

} // namespace NexusOptionsUI
