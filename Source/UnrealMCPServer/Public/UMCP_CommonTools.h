#pragma once

#include "UMCP_Types.h"

class FUMCP_CommonTools
{
public:
	void Register(class FUMCP_Server* Server);

private:
	bool GetProjectConfig(TSharedPtr<FJsonObject> arguments, TArray<FUMCP_CallToolResultContent>& OutContent);
	bool ExecuteConsoleCommand(TSharedPtr<FJsonObject> arguments, TArray<FUMCP_CallToolResultContent>& OutContent);
	bool GetLogFilePath(TSharedPtr<FJsonObject> arguments, TArray<FUMCP_CallToolResultContent>& OutContent);
};
