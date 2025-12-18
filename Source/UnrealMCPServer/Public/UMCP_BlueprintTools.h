#pragma once

#include "UMCP_Types.h"
#include "UMCP_BlueprintTools.generated.h"

// Blueprint Tools Parameter and Result Types

// SearchBlueprints tool parameters
USTRUCT()
struct UNREALMCPSERVER_API FUMCP_SearchBlueprintsParams
{
	GENERATED_BODY()

	UPROPERTY()
	FString searchType;

	UPROPERTY()
	FString searchTerm;

	UPROPERTY()
	FString packagePath;

	UPROPERTY()
	bool bRecursive = true;

	UPROPERTY()
	int32 maxResults = 0;

	UPROPERTY()
	int32 offset = 0;
};

// ExportBlueprintMarkdown tool parameters
USTRUCT()
struct UNREALMCPSERVER_API FUMCP_ExportBlueprintMarkdownParams
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FString> blueprintPaths; // Array of Blueprint object paths to export

	UPROPERTY()
	FString outputFolder; // Folder where markdown files will be saved
};

// ExportBlueprintMarkdown output
USTRUCT()
struct UNREALMCPSERVER_API FUMCP_ExportBlueprintMarkdownResult
{
	GENERATED_BODY()

	UPROPERTY()
	bool bSuccess;

	UPROPERTY()
	int32 exportedCount = 0;

	UPROPERTY()
	int32 failedCount = 0;

	UPROPERTY()
	TArray<FString> exportedPaths; // List of successfully exported markdown file paths

	UPROPERTY()
	TArray<FString> failedPaths; // List of Blueprint paths that failed to export

	UPROPERTY()
	FString error; // Error message if bSuccess is false
};

class FUMCP_BlueprintTools
{
public:
	void Register(class FUMCP_Server* Server);

private:
	bool SearchBlueprints(TSharedPtr<FJsonObject> arguments, TArray<FUMCP_CallToolResultContent>& OutContent);
	bool ExportBlueprintMarkdown(TSharedPtr<FJsonObject> arguments, TArray<FUMCP_CallToolResultContent>& OutContent);

	// Helper function to export a Blueprint asset to markdown text
	// Returns true on success, false on failure
	// On success, OutExportedText contains the exported markdown content
	// On failure, OutError contains an error message
	bool ExportBlueprintToMarkdown(const FString& ObjectPath, FString& OutExportedText, FString& OutError);
};

