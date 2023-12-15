// Copyright longlt00502@gmail.com 2023. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Delegates/IDelegateInstance.h"
#include "Modules/ModuleManager.h"

class FToolBarBuilder;
class FMenuBuilder;
class FExtender;
class FUICommandList;
class IAnimationEditor;
class UAnimSequence;
class SProgressBar;

DEFINE_LOG_CATEGORY_STATIC(FreeBoneSnapper, Log, All);

class FFreeBoneSnapperModule : public IModuleInterface
{
public:

	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
