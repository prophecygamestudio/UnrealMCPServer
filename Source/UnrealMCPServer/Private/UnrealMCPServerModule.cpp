#include "UnrealMCPServerModule.h"

#include "UMCP_CommonResources.h"
#include "UMCP_Server.h"
#include "UMCP_CommonTools.h"
#include "UMCP_AssetTools.h"
#include "UMCP_CommonResources.h"

// Define the log category
DEFINE_LOG_CATEGORY(LogUnrealMCPServer);

void FUnrealMCPServerModule::StartupModule()
{
	UE_LOG(LogUnrealMCPServer, Warning, TEXT("FUnrealMCPServerModule has started"));
	CommonTools = MakeUnique<FUMCP_CommonTools>();
	AssetTools = MakeUnique<FUMCP_AssetTools>();
	CommonResources = MakeUnique<FUMCP_CommonResources>();
	Server = MakeUnique<FUMCP_Server>();
	if (Server)
	{
		CommonTools->Register(Server.Get());
		AssetTools->Register(Server.Get());
		CommonResources->Register(Server.Get());
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
