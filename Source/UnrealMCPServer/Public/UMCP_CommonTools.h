#pragma once

#include "UMCP_Types.h"
#include "ILiveCodingModule.h"
#include "UMCP_CommonTools.generated.h"

// Common Tools Parameter and Result Types

// GetProjectConfig tool parameters
USTRUCT()
struct UNREALMCPSERVER_API FUMCP_GetProjectConfigParams
{
	GENERATED_BODY()
};

// GetProjectConfig output
USTRUCT()
struct UNREALMCPSERVER_API FUMCP_EngineVersionInfo
{
	GENERATED_BODY()

	UPROPERTY()
	FString full;

	UPROPERTY()
	int32 major;

	UPROPERTY()
	int32 minor;

	UPROPERTY()
	int32 patch;

	UPROPERTY()
	int32 changelist;
};

USTRUCT()
struct UNREALMCPSERVER_API FUMCP_ProjectPaths
{
	GENERATED_BODY()

	UPROPERTY()
	FString engineDir;

	UPROPERTY()
	FString projectDir;

	UPROPERTY()
	FString projectContentDir;

	UPROPERTY()
	FString projectLogDir;

	UPROPERTY()
	FString projectSavedDir;

	UPROPERTY()
	FString projectConfigDir;

	UPROPERTY()
	FString projectPluginsDir;

	UPROPERTY()
	FString engineContentDir;

	UPROPERTY()
	FString enginePluginsDir;
};

USTRUCT()
struct UNREALMCPSERVER_API FUMCP_GetProjectConfigResult
{
	GENERATED_BODY()

	UPROPERTY()
	FUMCP_EngineVersionInfo engineVersion;

	UPROPERTY()
	FUMCP_ProjectPaths paths;
};

// ExecuteConsoleCommand tool parameters
USTRUCT()
struct UNREALMCPSERVER_API FUMCP_ExecuteConsoleCommandParams
{
	GENERATED_BODY()

	UPROPERTY()
	FString command;
};

// ExecuteConsoleCommand output
USTRUCT()
struct UNREALMCPSERVER_API FUMCP_ExecuteConsoleCommandResult
{
	GENERATED_BODY()

	UPROPERTY()
	bool bSuccess;

	UPROPERTY()
	FString command;

	UPROPERTY()
	FString output;

	UPROPERTY()
	FString error;
};

// GetLogFilePath tool parameters
USTRUCT()
struct UNREALMCPSERVER_API FUMCP_GetLogFilePathParams
{
	GENERATED_BODY()
};

// GetLogFilePath output
USTRUCT()
struct UNREALMCPSERVER_API FUMCP_GetLogFilePathResult
{
	GENERATED_BODY()

	UPROPERTY()
	FString logFilePath;
};

// RequestEditorCompile tool parameters
USTRUCT()
struct UNREALMCPSERVER_API FUMCP_RequestEditorCompileParams
{
	GENERATED_BODY()

	// Optional: timeout in seconds (default: 300 seconds = 5 minutes)
	UPROPERTY()
	float timeoutSeconds = 300.0f;
};

// RequestEditorCompile output
USTRUCT()
struct UNREALMCPSERVER_API FUMCP_RequestEditorCompileResult
{
	GENERATED_BODY()

	UPROPERTY()
	bool bSuccess;

	UPROPERTY()
	bool bCompileStarted; // Whether compilation was successfully initiated

	UPROPERTY()
	FString status; // "completed", "failed", "timeout", "not_available", "error"

	UPROPERTY()
	FString buildLog; // Full build log output

	UPROPERTY()
	TArray<FString> errors; // Extracted error messages

	UPROPERTY()
	TArray<FString> warnings; // Extracted warning messages

	UPROPERTY()
	FString error; // Error message if bSuccess is false or status is "error"
};

class FUMCP_CommonTools
{
public:
	void Register(class FUMCP_Server* Server);

private:
	bool GetProjectConfig(TSharedPtr<FJsonObject> arguments, TArray<FUMCP_CallToolResultContent>& OutContent);
	bool ExecuteConsoleCommand(TSharedPtr<FJsonObject> arguments, TArray<FUMCP_CallToolResultContent>& OutContent);
	bool GetLogFilePath(TSharedPtr<FJsonObject> arguments, TArray<FUMCP_CallToolResultContent>& OutContent);
	bool RequestEditorCompile(TSharedPtr<FJsonObject> arguments, TArray<FUMCP_CallToolResultContent>& OutContent);
	bool HandleCompilationComplete(ILiveCodingModule* LiveCodingModule, ELiveCodingCompileResult CompileResult, FUMCP_RequestEditorCompileResult& Result, FUMCP_CallToolResultContent& Content);
};
