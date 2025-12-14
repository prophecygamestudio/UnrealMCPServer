#include "UnrealMCPServerModule.h"

#include "UMCP_CommonResources.h"
#include "UMCP_CommonPrompts.h"
#include "UMCP_Server.h"
#include "UMCP_CommonTools.h"
#include "UMCP_AssetTools.h"
#include "UMCP_BlueprintTools.h"

// Define the log category
DEFINE_LOG_CATEGORY(LogUnrealMCPServer);

void FUnrealMCPServerModule::StartupModule()
{
	UE_LOG(LogUnrealMCPServer, Warning, TEXT("FUnrealMCPServerModule has started"));
	CommonTools = MakeUnique<FUMCP_CommonTools>();
	AssetTools = MakeUnique<FUMCP_AssetTools>();
	BlueprintTools = MakeUnique<FUMCP_BlueprintTools>();
	CommonResources = MakeUnique<FUMCP_CommonResources>();
	CommonPrompts = MakeUnique<FUMCP_CommonPrompts>();
	Server = MakeUnique<FUMCP_Server>();
	if (Server)
	{
		CommonTools->Register(Server.Get());
		AssetTools->Register(Server.Get());
		BlueprintTools->Register(Server.Get());
		CommonResources->Register(Server.Get());
		CommonPrompts->Register(Server.Get());
		Server->StartServer();
	}
}

void FUnrealMCPServerModule::ShutdownModule()
{
	if (Server)
	{
		Server->StopServer();
		Server.Reset();
	}
	if (CommonResources)
	{
		CommonResources.Reset();
	}
	if (CommonPrompts)
	{
		CommonPrompts.Reset();
	}
	if (BlueprintTools)
	{
		BlueprintTools.Reset();
	}
	if (AssetTools)
	{
		AssetTools.Reset();
	}
	if (CommonTools)
	{
		CommonTools.Reset();
	}
	UE_LOG(LogUnrealMCPServer, Warning, TEXT("FUnrealMCPServerModule has shut down"));
}

IMPLEMENT_MODULE(FUnrealMCPServerModule, UnrealMCPServer)
