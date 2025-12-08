#pragma once

#include "UMCP_Types.h"

class FUMCP_CommonTools
{
public:
	void Register(class FUMCP_Server* Server);

private:
	bool SearchBlueprints(TSharedPtr<FJsonObject> arguments, TArray<FUMCP_CallToolResultContent>& OutContent);
	bool ExportAsset(TSharedPtr<FJsonObject> arguments, TArray<FUMCP_CallToolResultContent>& OutContent);
	bool ExportClassDefault(TSharedPtr<FJsonObject> arguments, TArray<FUMCP_CallToolResultContent>& OutContent);
	bool ImportAsset(TSharedPtr<FJsonObject> arguments, TArray<FUMCP_CallToolResultContent>& OutContent);
	bool QueryAsset(TSharedPtr<FJsonObject> arguments, TArray<FUMCP_CallToolResultContent>& OutContent);
	bool SearchAssets(TSharedPtr<FJsonObject> arguments, TArray<FUMCP_CallToolResultContent>& OutContent);
	bool GetProjectConfig(TSharedPtr<FJsonObject> arguments, TArray<FUMCP_CallToolResultContent>& OutContent);
};
