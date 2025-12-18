#pragma once

#include "UMCP_Types.h"
#include "UMCP_AssetTools.generated.h"

// Asset Tools Parameter and Result Types

// Asset Tools Parameter and Result Types

// ExportAsset tool parameters
USTRUCT()
struct UNREALMCPSERVER_API FUMCP_ExportAssetParams
{
	GENERATED_BODY()

	UPROPERTY()
	FString objectPath;

	UPROPERTY()
	FString format = TEXT("T3D");
};

// ExportAsset output
USTRUCT()
struct UNREALMCPSERVER_API FUMCP_ExportAssetResult
{
	GENERATED_BODY()

	UPROPERTY()
	bool bSuccess;

	UPROPERTY()
	FString objectPath;

	UPROPERTY()
	FString format;

	UPROPERTY()
	FString content; // The exported asset content

	UPROPERTY()
	FString error; // Error message if bSuccess is false
};

// BatchExportAssets tool parameters
USTRUCT()
struct UNREALMCPSERVER_API FUMCP_BatchExportAssetsParams
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FString> objectPaths;

	UPROPERTY()
	FString outputFolder;

	UPROPERTY()
	FString format = TEXT("T3D");
};

// BatchExportAssets output
USTRUCT()
struct UNREALMCPSERVER_API FUMCP_BatchExportAssetsResult
{
	GENERATED_BODY()

	UPROPERTY()
	bool bSuccess;

	UPROPERTY()
	int32 exportedCount = 0;

	UPROPERTY()
	int32 failedCount = 0;

	UPROPERTY()
	TArray<FString> exportedPaths; // List of successfully exported file paths

	UPROPERTY()
	TArray<FString> failedPaths; // List of asset paths that failed to export

	UPROPERTY()
	FString error; // Error message if bSuccess is false
};

// ExportClassDefault tool parameters
USTRUCT()
struct UNREALMCPSERVER_API FUMCP_ExportClassDefaultParams
{
	GENERATED_BODY()

	UPROPERTY()
	FString classPath;

	UPROPERTY()
	FString format = TEXT("T3D");
};

// ExportClassDefault output
USTRUCT()
struct UNREALMCPSERVER_API FUMCP_ExportClassDefaultResult
{
	GENERATED_BODY()

	UPROPERTY()
	bool bSuccess;

	UPROPERTY()
	FString classPath;

	UPROPERTY()
	FString format;

	UPROPERTY()
	FString content; // The exported class default object content

	UPROPERTY()
	FString error; // Error message if bSuccess is false
};

// ImportAsset tool parameters
USTRUCT()
struct UNREALMCPSERVER_API FUMCP_ImportAssetParams
{
	GENERATED_BODY()

	UPROPERTY()
	FString filePath; // Path to binary file (e.g., texture, mesh) - optional if t3dFilePath is provided

	UPROPERTY()
	FString t3dFilePath; // Path to T3D file for configuration - optional if filePath is provided

	UPROPERTY()
	FString packagePath;

	UPROPERTY()
	FString classPath;
};

// ImportAsset output
USTRUCT()
struct UNREALMCPSERVER_API FUMCP_ImportAssetResult
{
	GENERATED_BODY()

	UPROPERTY()
	bool bSuccess;

	UPROPERTY()
	int32 count = 0;

	UPROPERTY()
	FString filePath;

	UPROPERTY()
	FString packagePath;

	UPROPERTY()
	FString factoryClass;

	UPROPERTY()
	TArray<FString> importedObjects;

	UPROPERTY()
	FString error;
};

// QueryAsset tool parameters
USTRUCT()
struct UNREALMCPSERVER_API FUMCP_QueryAssetParams
{
	GENERATED_BODY()

	UPROPERTY()
	FString assetPath;

	UPROPERTY()
	bool bIncludeTags = false;
};

// QueryAsset output
USTRUCT()
struct UNREALMCPSERVER_API FUMCP_QueryAssetResult
{
	GENERATED_BODY()

	UPROPERTY()
	bool bExists;

	UPROPERTY()
	FString assetPath;

	UPROPERTY()
	FString assetName;

	UPROPERTY()
	FString packagePath;

	UPROPERTY()
	FString classPath;

	UPROPERTY()
	FString objectPath;

	UPROPERTY()
	TMap<FString, FString> tags;
};

// SearchAssets tool parameters
USTRUCT()
struct UNREALMCPSERVER_API FUMCP_SearchAssetsParams
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FString> packagePaths;

	UPROPERTY()
	TArray<FString> packageNames;

	UPROPERTY()
	TArray<FString> classPaths;

	UPROPERTY()
	bool bRecursive = true;

	UPROPERTY()
	bool bIncludeTags = false;

	UPROPERTY()
	int32 maxResults = 0;  // 0 means no limit

	UPROPERTY()
	int32 offset = 0;  // Offset for paging
};

// GetAssetDependencies tool parameters
USTRUCT()
struct UNREALMCPSERVER_API FUMCP_GetAssetDependenciesParams
{
	GENERATED_BODY()

	UPROPERTY()
	FString assetPath;

	UPROPERTY()
	bool bIncludeHardDependencies = true; // Hard dependencies (direct references)

	UPROPERTY()
	bool bIncludeSoftDependencies = false; // Soft dependencies (searchable references)
};

// GetAssetDependencies output
USTRUCT()
struct UNREALMCPSERVER_API FUMCP_GetAssetDependenciesResult
{
	GENERATED_BODY()

	UPROPERTY()
	bool bSuccess;

	UPROPERTY()
	FString assetPath;

	UPROPERTY()
	TArray<FString> dependencies; // Array of asset paths that this asset depends on

	UPROPERTY()
	int32 count = 0;

	UPROPERTY()
	FString error; // Error message if bSuccess is false
};

// GetAssetReferences tool parameters
USTRUCT()
struct UNREALMCPSERVER_API FUMCP_GetAssetReferencesParams
{
	GENERATED_BODY()

	UPROPERTY()
	FString assetPath;

	UPROPERTY()
	bool bIncludeHardReferences = true; // Hard references (direct references)

	UPROPERTY()
	bool bIncludeSoftReferences = false; // Soft references (searchable references)
};

// GetAssetReferences output
USTRUCT()
struct UNREALMCPSERVER_API FUMCP_GetAssetReferencesResult
{
	GENERATED_BODY()

	UPROPERTY()
	bool bSuccess;

	UPROPERTY()
	FString assetPath;

	UPROPERTY()
	TArray<FString> references; // Array of asset paths that reference this asset

	UPROPERTY()
	int32 count = 0;

	UPROPERTY()
	FString error; // Error message if bSuccess is false
};

// GetAssetDependencyTree tool parameters
USTRUCT()
struct UNREALMCPSERVER_API FUMCP_GetAssetDependencyTreeParams
{
	GENERATED_BODY()

	UPROPERTY()
	FString assetPath;

	UPROPERTY()
	int32 maxDepth = 10; // Maximum recursion depth to prevent infinite loops

	UPROPERTY()
	bool bIncludeHardDependencies = true;

	UPROPERTY()
	bool bIncludeSoftDependencies = false;
};

// Dependency tree node structure
USTRUCT()
struct UNREALMCPSERVER_API FUMCP_AssetDependencyNode
{
	GENERATED_BODY()

	UPROPERTY()
	FString assetPath;

	UPROPERTY()
	int32 depth = 0;

	UPROPERTY()
	TArray<FString> dependencies; // Direct dependencies (asset paths)
};

// GetAssetDependencyTree output
USTRUCT()
struct UNREALMCPSERVER_API FUMCP_GetAssetDependencyTreeResult
{
	GENERATED_BODY()

	UPROPERTY()
	bool bSuccess;

	UPROPERTY()
	FString assetPath;

	UPROPERTY()
	TArray<FUMCP_AssetDependencyNode> tree; // Flat list of nodes with depth information

	UPROPERTY()
	int32 totalNodes = 0;

	UPROPERTY()
	int32 maxDepthReached = 0;

	UPROPERTY()
	FString error; // Error message if bSuccess is false
};

class FUMCP_AssetTools
{
public:
	void Register(class FUMCP_Server* Server);

private:
	bool ExportAsset(TSharedPtr<FJsonObject> arguments, TArray<FUMCP_CallToolResultContent>& OutContent);
	bool BatchExportAssets(TSharedPtr<FJsonObject> arguments, TArray<FUMCP_CallToolResultContent>& OutContent);
	bool ExportClassDefault(TSharedPtr<FJsonObject> arguments, TArray<FUMCP_CallToolResultContent>& OutContent);
	bool ImportAsset(TSharedPtr<FJsonObject> arguments, TArray<FUMCP_CallToolResultContent>& OutContent);
	bool QueryAsset(TSharedPtr<FJsonObject> arguments, TArray<FUMCP_CallToolResultContent>& OutContent);
	bool SearchAssets(TSharedPtr<FJsonObject> arguments, TArray<FUMCP_CallToolResultContent>& OutContent);
	bool GetAssetDependencies(TSharedPtr<FJsonObject> arguments, TArray<FUMCP_CallToolResultContent>& OutContent);
	bool GetAssetReferences(TSharedPtr<FJsonObject> arguments, TArray<FUMCP_CallToolResultContent>& OutContent);
	bool GetAssetDependencyTree(TSharedPtr<FJsonObject> arguments, TArray<FUMCP_CallToolResultContent>& OutContent);

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

