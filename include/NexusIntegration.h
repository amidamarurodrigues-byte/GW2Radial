#pragma once

#include <Win.h>

struct AddonDefinition_t;

// Glue between the Nexus addon host and GW2Radial/GW2Common. Nexus owns the D3D11 hooks and the window procedure; this
// forwards frames and window messages to the (otherwise unchanged) addon core.
namespace NexusIntegration
{

// Must be called from DllMain, resources are loaded from this module.
void                SetModule(HMODULE module);

AddonDefinition_t*  GetDefinition();

} // namespace NexusIntegration
