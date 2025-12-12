#pragma once

#include "CoreMinimal.h"
#include "UMCP_Server.h"
#include "UMCP_CommonTools.h"
#include "UMCP_AssetTools.h"
#include "UMCP_BlueprintTools.h"
#include "UMCP_CommonResources.h"
#include "Modules/ModuleManager.h"

// Define a log category
UNREALMCPSERVER_API DECLARE_LOG_CATEGORY_EXTERN(LogUnrealMCPServer, Log, All);

class UNREALMCPSERVER_API FUnrealMCPServerModule : public IModuleInterface
{
public:
    /** IModuleInterface implementation */
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;
private:
	TUniquePtr<FUMCP_Server> Server;
	TUniquePtr<FUMCP_CommonTools> CommonTools;
	TUniquePtr<FUMCP_AssetTools> AssetTools;
	TUniquePtr<FUMCP_BlueprintTools> BlueprintTools;
	TUniquePtr<FUMCP_CommonResources> CommonResources;
};
