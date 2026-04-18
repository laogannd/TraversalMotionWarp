// Copyright (c) 2026 DGOne. All Rights Reserved.

#pragma once

#include "Modules/ModuleInterface.h"

class FTraversalMotionWarpModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
