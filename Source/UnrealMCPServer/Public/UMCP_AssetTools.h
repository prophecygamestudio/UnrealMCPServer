#pragma once

#include "UMCP_Types.h"

class FUMCP_AssetTools
{
public:
	void Register(class FUMCP_Server* Server);

private:
	bool SearchBlueprints(TSharedPtr<FJsonObject> arguments, TArray<FUMCP_CallToolResultContent>& OutContent);
	bool ExportAsset(TSharedPtr<FJsonObject> arguments, TArray<FUMCP_CallToolResultContent>& OutContent);
	bool BatchExportAssets(TSharedPtr<FJsonObject> arguments, TArray<FUMCP_CallToolResultContent>& OutContent);
	bool ExportClassDefault(TSharedPtr<FJsonObject> arguments, TArray<FUMCP_CallToolResultContent>& OutContent);
	bool ImportAsset(TSharedPtr<FJsonObject> arguments, TArray<FUMCP_CallToolResultContent>& OutContent);
	bool QueryAsset(TSharedPtr<FJsonObject> arguments, TArray<FUMCP_CallToolResultContent>& OutContent);
	bool SearchAssets(TSharedPtr<FJsonObject> arguments, TArray<FUMCP_CallToolResultContent>& OutContent);

	// Helper function to export a single asset to text
	// Returns true on success, false on failure
	// On success, OutExportedText contains the exported content
	// On failure, OutError contains an error message
	bool ExportAssetToText(const FString& ObjectPath, const FString& Format, FString& OutExportedText, FString& OutError);

	// Helper function to perform a single file import pass
	// Returns the imported object on success, nullptr on failure
	// The UFactory system will automatically determine the appropriate factory based on file type
	UObject* PerformImportPass(const FString& FilePath, UClass* ImportClass, const FString& PackagePath, const FString& ObjectName);
};

