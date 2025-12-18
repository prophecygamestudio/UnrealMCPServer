#include "UMCP_AssetTools.h"
#include "UMCP_Server.h"
#include "UMCP_Types.h"
#include "UMCP_T3DFallbackFactory.h"
#include "UnrealMCPServerModule.h"
#include "UObject/UnrealType.h"
#include "Exporters/Exporter.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Engine/Blueprint.h"
#include "K2Node.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/Package.h"
#include "Editor/UnrealEd/Public/UnrealEd.h"
#include "HAL/PlatformFilemanager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Factories/Factory.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Editor/UnrealEd/Public/Editor.h"
#include "Editor/UnrealEd/Public/AutomatedAssetImportData.h"
#include "Engine/Engine.h"
#include "Misc/ScopeExit.h"
#include "Internationalization/Text.h"
#include "Interfaces/IPluginManager.h"


namespace
{
	// Helper function to convert FAssetData to JSON
	void AssetDataToJson(const FAssetData& AssetData, TSharedPtr<FJsonObject> OutJson, bool bIncludeTags)
	{
		OutJson->SetBoolField(TEXT("exists"), true);
		OutJson->SetStringField(TEXT("assetPath"), AssetData.GetSoftObjectPath().ToString());
		OutJson->SetStringField(TEXT("assetName"), AssetData.AssetName.ToString());
		OutJson->SetStringField(TEXT("packagePath"), AssetData.PackagePath.ToString());
		OutJson->SetStringField(TEXT("classPath"), AssetData.AssetClassPath.ToString());
		OutJson->SetStringField(TEXT("objectPath"), AssetData.GetObjectPathString());
		
		if (bIncludeTags)
		{
			TSharedPtr<FJsonObject> TagsJson = MakeShareable(new FJsonObject);
			for (const auto& TagPair : AssetData.TagsAndValues)
			{
				FString TagValue = TagPair.Value.GetValue();
				TagsJson->SetStringField(TagPair.Key.ToString(), TagValue);
			}
			OutJson->SetObjectField(TEXT("tags"), TagsJson);
		}
	}
	
	// Helper function to resolve class path string to FTopLevelAssetPath
	FTopLevelAssetPath ResolveClassPath(const FString& ClassPathStr)
	{
		// Try direct conversion first (handles full paths like /Script/Engine.Blueprint)
		FTopLevelAssetPath ClassPath = FTopLevelAssetPath(ClassPathStr);
		if (ClassPath.IsValid())
		{
			return ClassPath;
		}

		// Return invalid path if we couldn't resolve it
		return FTopLevelAssetPath();
	}
	
	// Helper function to check if a package name is a partial match (contains wildcards or is not a full path)
	bool IsPartialPackageName(const FString& PackageName)
	{
		// Check for wildcard characters
		if (PackageName.Contains(TEXT("*")) || PackageName.Contains(TEXT("?")))
		{
			return true;
		}
		
		// Check if it's not a full package path (doesn't start with /)
		// Full package paths start with /Game/, /Engine/, etc.
		if (!PackageName.StartsWith(TEXT("/")))
		{
			return true;
		}
		
		// If it's a full path but contains a partial name pattern (e.g., "/Game/BP_*")
		// we'll treat it as partial if it has wildcards (already checked above)
		// or if it doesn't look like a complete package name
		
		return false;
	}
	
	// Helper function to match a package name against a pattern
	// Supports wildcards (* and ?) and substring matching
	bool MatchesPackageNamePattern(const FString& PackageName, const FString& Pattern)
	{
		// Get the full package name (package path + asset name)
		FString FullPackageName = PackageName;
		
		// If pattern contains wildcards, use wildcard matching
		if (Pattern.Contains(TEXT("*")) || Pattern.Contains(TEXT("?")))
		{
			return FullPackageName.MatchesWildcard(Pattern, ESearchCase::IgnoreCase);
		}
		
		// Otherwise, use case-insensitive substring matching
		return FullPackageName.Contains(Pattern, ESearchCase::IgnoreCase);
	}
}


void FUMCP_AssetTools::Register(class FUMCP_Server* Server)
{
	{
		FUMCP_ToolDefinition Tool;
		Tool.name = TEXT("export_asset");
		
		// Check if BP2AI plugin is available for markdown export support
		TSharedPtr<IPlugin> BP2AIPlugin = IPluginManager::Get().FindPlugin(TEXT("BP2AI"));
		bool bBP2AIAvailable = BP2AIPlugin.IsValid() && BP2AIPlugin->IsEnabled();
		
		// Build description based on plugin availability
		FString Description = TEXT("Export a single UObject to a specified format (defaults to T3D). Exportable asset types include: StaticMesh, Texture2D, Material, Sound, Animation, and most UObject-derived classes. Returns the exported content as a string. T3D format provides human-readable text representation of Unreal objects. ");
		if (bBP2AIAvailable)
		{
			Description += TEXT("Markdown format provides structured documentation of asset properties. ");
		}
		Description += TEXT("IMPORTANT: This tool will fail if used with Blueprint assets. Blueprints must be exported using batch_export_assets instead, as Blueprint exports generate responses too large to be parsed. For large exports, consider using batch_export_assets which saves to files.");
		Tool.description = Description;
		
		Tool.DoToolCall.BindRaw(this, &FUMCP_AssetTools::ExportAsset);
		
		// Generate input schema from USTRUCT with descriptions
		TMap<FString, FString> InputDescriptions;
		InputDescriptions.Add(TEXT("objectPath"), TEXT("The Unreal Engine object path to export. Format: '/Game/Folder/AssetName' or '/Game/Folder/AssetName.AssetName'. Examples: '/Game/MyAsset', '/Game/Textures/MyTexture.MyTexture', '/Engine/EditorMaterials/GridMaterial'. Blueprint assets are not supported and will fail. Use batch_export_assets for Blueprint assets. Use query_asset first to verify the asset exists."));
		FString FormatDescription = TEXT("The export format. 'T3D': Human-readable text representation (default). ");
		if (bBP2AIAvailable)
		{
			FormatDescription += TEXT("'md': Structured markdown documentation. ");
		}
		FormatDescription += TEXT("Defaults to 'T3D' if not specified. Other formats may be available depending on the asset type (e.g., 'OBJ' for meshes).");
		InputDescriptions.Add(TEXT("format"), FormatDescription);
		TArray<FString> InputRequired;
		InputRequired.Add(TEXT("objectPath"));
		Tool.inputSchema = UMCP_GenerateJsonSchemaFromStruct<FUMCP_ExportAssetParams>(InputDescriptions, InputRequired);
		
		// Generate output schema from USTRUCT with descriptions
		TMap<FString, FString> OutputDescriptions;
		OutputDescriptions.Add(TEXT("bSuccess"), TEXT("Whether the export was successful"));
		OutputDescriptions.Add(TEXT("objectPath"), TEXT("The path to the object that was exported"));
		FString OutputFormatDescription = TEXT("The export format used (e.g., 'T3D'");
		if (bBP2AIAvailable)
		{
			OutputFormatDescription += TEXT(", 'md'");
		}
		OutputFormatDescription += TEXT(", 'OBJ')");
		OutputDescriptions.Add(TEXT("format"), OutputFormatDescription);
		OutputDescriptions.Add(TEXT("content"), TEXT("The exported asset content in the specified format. The format varies depending on the exporter type and object type."));
		OutputDescriptions.Add(TEXT("error"), TEXT("Error message if bSuccess is false"));
		TArray<FString> OutputRequired;
		OutputRequired.Add(TEXT("bSuccess"));
		OutputRequired.Add(TEXT("objectPath"));
		TSharedPtr<FJsonObject> ExportOutputSchema = UMCP_GenerateJsonSchemaFromStruct<FUMCP_ExportAssetResult>(OutputDescriptions, OutputRequired);
		if (ExportOutputSchema.IsValid())
		{
			Tool.outputSchema = ExportOutputSchema;
		}
		else
		{
			UE_LOG(LogUnrealMCPServer, Error, TEXT("Failed to generate outputSchema for export_asset tool"));
		}
		Server->RegisterTool(MoveTemp(Tool));
	}
	
	{
		FUMCP_ToolDefinition Tool;
		Tool.name = TEXT("batch_export_assets");
		
		// Check if BP2AI plugin is available for markdown export support
		TSharedPtr<IPlugin> BP2AIPlugin = IPluginManager::Get().FindPlugin(TEXT("BP2AI"));
		bool bBP2AIAvailable = BP2AIPlugin.IsValid() && BP2AIPlugin->IsEnabled();
		
		// Build description based on plugin availability
		FString Description = TEXT("Export multiple assets to files in a specified folder. Returns a list of the exported file paths. Required for Blueprint assets, as export_asset will fail for Blueprints due to response size limitations. Use this when exporting multiple assets of any type. Files are saved to disk at the specified output folder path. Format defaults to T3D. Each asset is exported to a separate file named after the asset. Returns array of successfully exported file paths. Failed exports are not included in the return value. ");
		Description += TEXT("NOTE: For Blueprint graph inspection, use export_blueprint_markdown instead, which is specifically designed for that purpose and provides clearer workflow guidance.");
		Tool.description = Description;
		Tool.DoToolCall.BindRaw(this, &FUMCP_AssetTools::BatchExportAssets);
		
		// Generate input schema from USTRUCT with descriptions
		TMap<FString, FString> InputDescriptions;
		InputDescriptions.Add(TEXT("objectPaths"), TEXT("Array of Unreal Engine object paths to export. Each path should be in format '/Game/Folder/AssetName' or '/Game/Folder/AssetName.AssetName'. Examples: ['/Game/MyAsset', '/Game/Blueprints/BP_Player.BP_Player']. Can include Blueprint assets (unlike export_asset)."));
		InputDescriptions.Add(TEXT("outputFolder"), TEXT("The absolute or relative folder path where exported files should be saved. Examples: 'C:/Exports/Blueprints', './Exports', '/tmp/exports'. The folder will be created if it doesn't exist. Each asset is exported to a separate file named after the asset (e.g., 'BP_Player.T3D', 'MyTexture.T3D', 'BP_Player.md' for markdown format)."));
		FString FormatDescription = TEXT("The export format. Defaults to 'T3D' if not specified. Examples: 'T3D' (human-readable text), 'OBJ' (for meshes). ");
		if (bBP2AIAvailable)
		{
			FormatDescription += TEXT("'md' (markdown): Available for assets that support markdown export. ");
		}
		FormatDescription += TEXT("Format must be supported by the exporter for each asset type. NOTE: For Blueprint markdown export, use export_blueprint_markdown instead.");
		InputDescriptions.Add(TEXT("format"), FormatDescription);
		TArray<FString> InputRequired;
		InputRequired.Add(TEXT("objectPaths"));
		InputRequired.Add(TEXT("outputFolder"));
		Tool.inputSchema = UMCP_GenerateJsonSchemaFromStruct<FUMCP_BatchExportAssetsParams>(InputDescriptions, InputRequired);
		
		// Generate output schema from USTRUCT with descriptions
		TMap<FString, FString> OutputDescriptions;
		OutputDescriptions.Add(TEXT("bSuccess"), TEXT("Whether the batch export operation was successful overall"));
		OutputDescriptions.Add(TEXT("exportedCount"), TEXT("Number of assets successfully exported"));
		OutputDescriptions.Add(TEXT("failedCount"), TEXT("Number of assets that failed to export"));
		OutputDescriptions.Add(TEXT("exportedPaths"), TEXT("Array of file paths for successfully exported assets"));
		OutputDescriptions.Add(TEXT("failedPaths"), TEXT("Array of object paths that failed to export"));
		OutputDescriptions.Add(TEXT("error"), TEXT("Error message if bSuccess is false"));
		TArray<FString> OutputRequired;
		OutputRequired.Add(TEXT("bSuccess"));
		OutputRequired.Add(TEXT("exportedCount"));
		OutputRequired.Add(TEXT("failedCount"));
		TSharedPtr<FJsonObject> BatchExportOutputSchema = UMCP_GenerateJsonSchemaFromStruct<FUMCP_BatchExportAssetsResult>(OutputDescriptions, OutputRequired);
		if (BatchExportOutputSchema.IsValid())
		{
			Tool.outputSchema = BatchExportOutputSchema;
		}
		else
		{
			UE_LOG(LogUnrealMCPServer, Error, TEXT("Failed to generate outputSchema for batch_export_assets tool"));
		}
		Server->RegisterTool(MoveTemp(Tool));
	}
	
	{
		FUMCP_ToolDefinition Tool;
		Tool.name = TEXT("export_class_default");
		Tool.description = TEXT("Export the class default object (CDO) for a given class path. This allows determining default values for a class, since exporting instances of objects do not print values that are identical to the default value. Use this to understand default property values for Unreal classes. Useful for comparing instance values against defaults. Returns T3D format by default, showing all default property values for the class.");
		Tool.DoToolCall.BindRaw(this, &FUMCP_AssetTools::ExportClassDefault);
		
		// Generate input schema from USTRUCT with descriptions
		TMap<FString, FString> InputDescriptions;
		InputDescriptions.Add(TEXT("classPath"), TEXT("The class path to export the default object for. C++ class format: '/Script/Engine.Actor', '/Script/Engine.Pawn', '/Script/Engine.Character'. Blueprint class format: '/Game/Blueprints/BP_Player.BP_Player_C' (note the '_C' suffix for Blueprint classes). Examples: '/Script/Engine.Actor', '/Script/Engine.Texture2D', '/Game/Blueprints/BP_Enemy.BP_Enemy_C'."));
		InputDescriptions.Add(TEXT("format"), TEXT("The export format. Defaults to 'T3D' if not specified. 'T3D' provides human-readable text showing all default property values. Other formats may be available depending on the class type."));
		TArray<FString> InputRequired;
		InputRequired.Add(TEXT("classPath"));
		Tool.inputSchema = UMCP_GenerateJsonSchemaFromStruct<FUMCP_ExportClassDefaultParams>(InputDescriptions, InputRequired);
		
		// Generate output schema from USTRUCT with descriptions
		TMap<FString, FString> OutputDescriptions;
		OutputDescriptions.Add(TEXT("bSuccess"), TEXT("Whether the export was successful"));
		OutputDescriptions.Add(TEXT("classPath"), TEXT("The class path that was exported"));
		OutputDescriptions.Add(TEXT("format"), TEXT("The export format used (e.g., 'T3D', 'OBJ')"));
		OutputDescriptions.Add(TEXT("content"), TEXT("The exported class default object content in the specified format."));
		OutputDescriptions.Add(TEXT("error"), TEXT("Error message if bSuccess is false"));
		TArray<FString> OutputRequired;
		OutputRequired.Add(TEXT("bSuccess"));
		OutputRequired.Add(TEXT("classPath"));
		TSharedPtr<FJsonObject> ExportClassDefaultOutputSchema = UMCP_GenerateJsonSchemaFromStruct<FUMCP_ExportClassDefaultResult>(OutputDescriptions, OutputRequired);
		if (ExportClassDefaultOutputSchema.IsValid())
		{
			Tool.outputSchema = ExportClassDefaultOutputSchema;
		}
		else
		{
			UE_LOG(LogUnrealMCPServer, Error, TEXT("Failed to generate outputSchema for export_class_default tool"));
		}
		Server->RegisterTool(MoveTemp(Tool));
	}
	
	{
		FUMCP_ToolDefinition Tool;
		Tool.name = TEXT("import_asset");
		Tool.description = TEXT("Import a file to create or update a UObject. The file type is automatically detected based on available factories. Import binary files (textures, meshes, sounds) or T3D files to create/update Unreal assets. Supported binary formats: .fbx, .obj (meshes), .png, .jpg, .tga (textures), .wav, .mp3 (sounds). T3D files can be used to import from T3D format or to configure imported objects. If asset exists at packagePath, it will be updated. Otherwise, a new asset is created. At least one of filePath (binary) or t3dFilePath (T3D) must be provided.");
		Tool.DoToolCall.BindRaw(this, &FUMCP_AssetTools::ImportAsset);
		
		// Generate input schema from USTRUCT with descriptions
		TMap<FString, FString> InputDescriptions;
		InputDescriptions.Add(TEXT("filePath"), TEXT("The absolute or relative path to the binary file to import. Supported formats: .fbx, .obj (meshes), .png, .jpg, .tga (textures), .wav, .mp3 (sounds). Examples: 'C:/Models/MyMesh.fbx', './Textures/MyTexture.png'. Optional if t3dFilePath is provided. At least one of filePath or t3dFilePath must be specified."));
		InputDescriptions.Add(TEXT("t3dFilePath"), TEXT("The absolute or relative path to the T3D file for configuration. T3D files can be used to import from T3D format or to configure imported objects. Examples: 'C:/Exports/MyAsset.T3D', './Config/MyAsset.T3D'. Optional if filePath is provided. At least one of filePath or t3dFilePath must be specified."));
		InputDescriptions.Add(TEXT("packagePath"), TEXT("The full object path where the object should be created, including the object name. Format: '/Game/MyFolder/MyAsset.MyAsset' (include object name after the dot). Examples: '/Game/MyAsset.MyAsset', '/Game/Textures/MyTexture.MyTexture', '/Game/Meshes/MyMesh.MyMesh'. If asset exists at this path, it will be updated. Otherwise, a new asset is created."));
		InputDescriptions.Add(TEXT("classPath"), TEXT("The class path of the object to import. C++ class format: '/Script/Engine.Texture2D', '/Script/Engine.StaticMesh', '/Script/Engine.SoundWave'. Blueprint class format: '/Game/Blueprints/BP_Player.BP_Player_C'. Examples: '/Script/Engine.Texture2D' (for textures), '/Script/Engine.StaticMesh' (for meshes), '/Script/Engine.SoundWave' (for sounds)."));
		TArray<FString> InputRequired;
		InputRequired.Add(TEXT("packagePath"));
		InputRequired.Add(TEXT("classPath"));
		Tool.inputSchema = UMCP_GenerateJsonSchemaFromStruct<FUMCP_ImportAssetParams>(InputDescriptions, InputRequired);
		
		// Generate output schema from USTRUCT with descriptions
		TMap<FString, FString> OutputDescriptions;
		OutputDescriptions.Add(TEXT("bSuccess"), TEXT("Whether the import was successful"));
		OutputDescriptions.Add(TEXT("count"), TEXT("Number of objects imported (if bSuccess is true)"));
		OutputDescriptions.Add(TEXT("filePath"), TEXT("The absolute file path that was imported"));
		OutputDescriptions.Add(TEXT("packagePath"), TEXT("The package path where objects were imported"));
		OutputDescriptions.Add(TEXT("factoryClass"), TEXT("The factory class name used for import"));
		OutputDescriptions.Add(TEXT("importedObjects"), TEXT("Array of object paths for imported objects (if bSuccess is true)"));
		OutputDescriptions.Add(TEXT("error"), TEXT("Error message if bSuccess is false"));
		TArray<FString> OutputRequired;
		OutputRequired.Add(TEXT("bSuccess"));
		TSharedPtr<FJsonObject> ImportOutputSchema = UMCP_GenerateJsonSchemaFromStruct<FUMCP_ImportAssetResult>(OutputDescriptions, OutputRequired);
		if (ImportOutputSchema.IsValid())
		{
			Tool.outputSchema = ImportOutputSchema;
		}
		else
		{
			UE_LOG(LogUnrealMCPServer, Error, TEXT("Failed to generate outputSchema for import_asset tool"));
		}
		Server->RegisterTool(MoveTemp(Tool));
	}
	
	{
		FUMCP_ToolDefinition Tool;
		Tool.name = TEXT("query_asset");
		Tool.description = TEXT("Query a single asset to check if it exists and get its basic information from the asset registry. Use this before export_asset or import_asset to verify an asset exists. Faster than export_asset for simple existence checks. Returns asset path, name, class, package path, and optionally tags. Returns error if asset doesn't exist.");
		Tool.DoToolCall.BindRaw(this, &FUMCP_AssetTools::QueryAsset);
		
		// Generate input schema from USTRUCT with descriptions
		TMap<FString, FString> InputDescriptions;
		InputDescriptions.Add(TEXT("assetPath"), TEXT("Single asset path to query. Format: '/Game/MyAsset' or '/Game/MyFolder/MyAsset' or '/Game/MyFolder/MyAsset.MyAsset'. Examples: '/Game/MyAsset', '/Game/Blueprints/BP_Player', '/Engine/EditorMaterials/GridMaterial'. Must start with '/Game/' or '/Engine/'. Asset must exist in the project."));
		InputDescriptions.Add(TEXT("bIncludeTags"), TEXT("Whether to include asset tags in the response. Defaults to false. Set to true to get additional metadata tags associated with the asset (e.g., 'ParentClass' for Blueprints, 'TextureGroup' for textures)."));
		TArray<FString> InputRequired;
		InputRequired.Add(TEXT("assetPath"));
		Tool.inputSchema = UMCP_GenerateJsonSchemaFromStruct<FUMCP_QueryAssetParams>(InputDescriptions, InputRequired);
		
		// Generate output schema from USTRUCT with descriptions
		TMap<FString, FString> OutputDescriptions;
		OutputDescriptions.Add(TEXT("bExists"), TEXT("Whether the asset exists"));
		OutputDescriptions.Add(TEXT("assetPath"), TEXT("The asset path that was queried"));
		OutputDescriptions.Add(TEXT("assetName"), TEXT("Name of the asset (if bExists is true)"));
		OutputDescriptions.Add(TEXT("packagePath"), TEXT("Package path of the asset (if bExists is true)"));
		OutputDescriptions.Add(TEXT("classPath"), TEXT("Class path of the asset (if bExists is true)"));
		OutputDescriptions.Add(TEXT("objectPath"), TEXT("Full object path of the asset (if bExists is true)"));
		OutputDescriptions.Add(TEXT("tags"), TEXT("Asset tags (if bIncludeTags was true and bExists is true)"));
		TArray<FString> OutputRequired;
		OutputRequired.Add(TEXT("bExists"));
		OutputRequired.Add(TEXT("assetPath"));
		TSharedPtr<FJsonObject> QueryOutputSchema = UMCP_GenerateJsonSchemaFromStruct<FUMCP_QueryAssetResult>(OutputDescriptions, OutputRequired);
		if (QueryOutputSchema.IsValid())
		{
			Tool.outputSchema = QueryOutputSchema;
		}
		else
		{
			UE_LOG(LogUnrealMCPServer, Error, TEXT("Failed to generate outputSchema for query_asset tool"));
		}
		Server->RegisterTool(MoveTemp(Tool));
	}
	
	{
		FUMCP_ToolDefinition Tool;
		Tool.name = TEXT("search_assets");
		Tool.description = TEXT("Search for assets by package paths or package names, optionally filtered by class. Returns an array of asset information from the asset registry. More flexible than search_blueprints as it works with all asset types. REQUIRED: At least one of 'packagePaths' or 'packageNames' must be provided (non-empty array). Use packagePaths to search directories (e.g., '/Game/Blueprints' searches all assets in that folder), packageNames for exact or partial package matches (supports wildcards * and ?, or substring matching), and classPaths to filter by asset type (e.g., textures only). Returns array of asset information. Use bIncludeTags=true to get additional metadata tags. Use maxResults and offset for paging through large result sets. For large searches, use maxResults to limit results and offset for paging.");
		Tool.DoToolCall.BindRaw(this, &FUMCP_AssetTools::SearchAssets);
		
		// Generate input schema from USTRUCT with descriptions
		TMap<FString, FString> InputDescriptions;
		InputDescriptions.Add(TEXT("packagePaths"), TEXT("REQUIRED (if packageNames is empty): Array of directory/package paths to search for assets. Examples: ['/Game/Blueprints', '/Game/Materials', '/Game/Textures']. Uses Unreal's path format. Searches all assets in specified folders (recursive by default). At least one of packagePaths or packageNames must be provided (non-empty array). For large directories, use maxResults and offset for paging."));
		InputDescriptions.Add(TEXT("packageNames"), TEXT("REQUIRED (if packagePaths is empty): Array of package names to search for. Supports both exact matches and partial matches. Examples: ['MyAsset', '/Game/MyAsset', '/Game/Blueprints/BP_Player'] for exact matches, ['BP_*', '*Player*', 'MyAsset'] for partial matches. Partial matching supports: (1) Wildcards: * (matches any characters) and ? (matches single character), e.g., 'BP_*' matches all packages starting with 'BP_'; (2) Substring matching: partial names without wildcards will match if the package name contains the substring (case-insensitive), e.g., 'Player' matches '/Game/Blueprints/BP_Player'. Can be used instead of or in combination with packagePaths. At least one of packagePaths or packageNames must be provided (non-empty array). More targeted than packagePaths as it searches for specific packages."));
		InputDescriptions.Add(TEXT("classPaths"), TEXT("Array of class paths to filter by. Examples: ['/Script/Engine.Blueprint', '/Script/Engine.Texture2D', '/Script/Engine.StaticMesh']. If empty, searches all asset types. C++ classes: '/Script/Engine.ClassName'. Blueprint classes: '/Game/Blueprints/BP_Player.BP_Player_C'. Recommended for large searches to reduce result set size."));
		InputDescriptions.Add(TEXT("bRecursive"), TEXT("Whether to search recursively in subdirectories. Defaults to true. Set to false to search only the specified packagePaths directories without subdirectories."));
		InputDescriptions.Add(TEXT("bIncludeTags"), TEXT("Whether to include asset tags in the response. Defaults to false. Set to true to get additional metadata tags for each asset (e.g., 'ParentClass' for Blueprints, 'TextureGroup' for textures, 'AssetImportData' for imported assets)."));
		InputDescriptions.Add(TEXT("maxResults"), TEXT("Maximum number of results to return. Defaults to 0 (no limit). Use with offset for paging through large result sets. Recommended for large searches to limit response size."));
		InputDescriptions.Add(TEXT("offset"), TEXT("Number of results to skip before returning results. Defaults to 0. Use with maxResults for paging: first page uses offset=0, second page uses offset=maxResults, etc."));
		TArray<FString> InputRequired;
		// packagePaths is required only if packageNames is empty
		Tool.inputSchema = UMCP_GenerateJsonSchemaFromStruct<FUMCP_SearchAssetsParams>(InputDescriptions, InputRequired);
		
		// Output schema has complex nested asset objects, so we'll keep it as manual JSON for now
		// TODO: Could create USTRUCT for output and generate schema
		FString SearchAssetsOutputSchema = TEXT("{")
			TEXT("\"type\":\"object\",")
			TEXT("\"properties\":{")
			TEXT("\"assets\":{\"type\":\"array\",\"description\":\"Array of asset information objects\",\"items\":{\"type\":\"object\",\"properties\":{")
			TEXT("\"exists\":{\"type\":\"boolean\"},")
			TEXT("\"assetPath\":{\"type\":\"string\"},")
			TEXT("\"assetName\":{\"type\":\"string\"},")
			TEXT("\"packagePath\":{\"type\":\"string\"},")
			TEXT("\"classPath\":{\"type\":\"string\"},")
			TEXT("\"objectPath\":{\"type\":\"string\"},")
			TEXT("\"tags\":{\"type\":\"object\",\"additionalProperties\":{\"type\":\"string\"}}")
			TEXT("}}},")
			TEXT("\"count\":{\"type\":\"number\",\"description\":\"Total number of assets found\"}")
			TEXT("},")
			TEXT("\"required\":[\"assets\",\"count\"]")
			TEXT("}");
		TSharedPtr<FJsonObject> ParsedSchema = UMCP_FromJsonStr(SearchAssetsOutputSchema);
		if (ParsedSchema.IsValid())
		{
			Tool.outputSchema = ParsedSchema;
		}
		else
		{
			UE_LOG(LogUnrealMCPServer, Error, TEXT("Failed to parse outputSchema for search_assets tool"));
		}
		Server->RegisterTool(MoveTemp(Tool));
	}
	
	{
		FUMCP_ToolDefinition Tool;
		Tool.name = TEXT("get_asset_dependencies");
		Tool.description = TEXT("Get all assets that a specified asset depends on. Returns an array of asset paths that the specified asset depends on. Use this to understand what assets an asset requires, which is useful for impact analysis, refactoring safety, and understanding asset relationships. Very useful when doing asset searches and queries with existing tools. Supports both hard dependencies (direct references) and soft dependencies (searchable references).");
		Tool.DoToolCall.BindRaw(this, &FUMCP_AssetTools::GetAssetDependencies);
		
		// Generate input schema from USTRUCT with descriptions
		TMap<FString, FString> InputDescriptions;
		InputDescriptions.Add(TEXT("assetPath"), TEXT("The asset path to get dependencies for. Format: '/Game/Folder/AssetName' or '/Game/Folder/AssetName.AssetName'. Examples: '/Game/MyAsset', '/Game/Blueprints/BP_Player', '/Engine/EditorMaterials/GridMaterial'. Must start with '/Game/' or '/Engine/'. Asset must exist in the project."));
		InputDescriptions.Add(TEXT("bIncludeHardDependencies"), TEXT("Whether to include hard dependencies (direct references). Defaults to true. Hard dependencies are assets that are directly referenced by the asset."));
		InputDescriptions.Add(TEXT("bIncludeSoftDependencies"), TEXT("Whether to include soft dependencies (searchable references). Defaults to false. Soft dependencies are assets that are referenced via searchable references (e.g., string-based asset references)."));
		TArray<FString> InputRequired;
		InputRequired.Add(TEXT("assetPath"));
		Tool.inputSchema = UMCP_GenerateJsonSchemaFromStruct<FUMCP_GetAssetDependenciesParams>(InputDescriptions, InputRequired);
		
		// Generate output schema from USTRUCT with descriptions
		TMap<FString, FString> OutputDescriptions;
		OutputDescriptions.Add(TEXT("bSuccess"), TEXT("Whether the operation completed successfully"));
		OutputDescriptions.Add(TEXT("assetPath"), TEXT("The asset path that was queried"));
		OutputDescriptions.Add(TEXT("dependencies"), TEXT("Array of asset paths that this asset depends on"));
		OutputDescriptions.Add(TEXT("count"), TEXT("Number of dependencies found"));
		OutputDescriptions.Add(TEXT("error"), TEXT("Error message if bSuccess is false"));
		TArray<FString> OutputRequired;
		OutputRequired.Add(TEXT("bSuccess"));
		OutputRequired.Add(TEXT("assetPath"));
		OutputRequired.Add(TEXT("dependencies"));
		OutputRequired.Add(TEXT("count"));
		TSharedPtr<FJsonObject> GetAssetDependenciesOutputSchema = UMCP_GenerateJsonSchemaFromStruct<FUMCP_GetAssetDependenciesResult>(OutputDescriptions, OutputRequired);
		if (GetAssetDependenciesOutputSchema.IsValid())
		{
			Tool.outputSchema = GetAssetDependenciesOutputSchema;
		}
		else
		{
			UE_LOG(LogUnrealMCPServer, Error, TEXT("Failed to generate outputSchema for get_asset_dependencies tool"));
		}
		Server->RegisterTool(MoveTemp(Tool));
	}
	
	{
		FUMCP_ToolDefinition Tool;
		Tool.name = TEXT("get_asset_references");
		Tool.description = TEXT("Get all assets that reference a specified asset. Returns an array of asset paths that reference the specified asset. Use this to understand what assets depend on this asset, which is critical for impact analysis, refactoring safety, and unused asset detection. Very useful when doing asset searches and queries with existing tools. Supports both hard references (direct references) and soft references (searchable references).");
		Tool.DoToolCall.BindRaw(this, &FUMCP_AssetTools::GetAssetReferences);
		
		// Generate input schema from USTRUCT with descriptions
		TMap<FString, FString> InputDescriptions;
		InputDescriptions.Add(TEXT("assetPath"), TEXT("The asset path to get references for. Format: '/Game/Folder/AssetName' or '/Game/Folder/AssetName.AssetName'. Examples: '/Game/MyAsset', '/Game/Blueprints/BP_Player', '/Engine/EditorMaterials/GridMaterial'. Must start with '/Game/' or '/Engine/'. Asset must exist in the project."));
		InputDescriptions.Add(TEXT("bIncludeHardReferences"), TEXT("Whether to include hard references (direct references). Defaults to true. Hard references are assets that directly reference this asset."));
		InputDescriptions.Add(TEXT("bIncludeSoftReferences"), TEXT("Whether to include soft references (searchable references). Defaults to false. Soft references are assets that reference this asset via searchable references (e.g., string-based asset references)."));
		TArray<FString> InputRequired;
		InputRequired.Add(TEXT("assetPath"));
		Tool.inputSchema = UMCP_GenerateJsonSchemaFromStruct<FUMCP_GetAssetReferencesParams>(InputDescriptions, InputRequired);
		
		// Generate output schema from USTRUCT with descriptions
		TMap<FString, FString> OutputDescriptions;
		OutputDescriptions.Add(TEXT("bSuccess"), TEXT("Whether the operation completed successfully"));
		OutputDescriptions.Add(TEXT("assetPath"), TEXT("The asset path that was queried"));
		OutputDescriptions.Add(TEXT("references"), TEXT("Array of asset paths that reference this asset"));
		OutputDescriptions.Add(TEXT("count"), TEXT("Number of references found"));
		OutputDescriptions.Add(TEXT("error"), TEXT("Error message if bSuccess is false"));
		TArray<FString> OutputRequired;
		OutputRequired.Add(TEXT("bSuccess"));
		OutputRequired.Add(TEXT("assetPath"));
		OutputRequired.Add(TEXT("references"));
		OutputRequired.Add(TEXT("count"));
		TSharedPtr<FJsonObject> GetAssetReferencesOutputSchema = UMCP_GenerateJsonSchemaFromStruct<FUMCP_GetAssetReferencesResult>(OutputDescriptions, OutputRequired);
		if (GetAssetReferencesOutputSchema.IsValid())
		{
			Tool.outputSchema = GetAssetReferencesOutputSchema;
		}
		else
		{
			UE_LOG(LogUnrealMCPServer, Error, TEXT("Failed to generate outputSchema for get_asset_references tool"));
		}
		Server->RegisterTool(MoveTemp(Tool));
	}
	
	{
		FUMCP_ToolDefinition Tool;
		Tool.name = TEXT("get_asset_dependency_tree");
		Tool.description = TEXT("Get the complete dependency tree for a specified asset. Returns a recursive tree structure showing all dependencies and their dependencies. Use this for complete dependency mapping and recursive analysis. The tree includes depth information for each node. Very useful when doing asset searches and queries with existing tools. Supports both hard dependencies (direct references) and soft dependencies (searchable references). Use maxDepth to limit recursion depth and prevent infinite loops.");
		Tool.DoToolCall.BindRaw(this, &FUMCP_AssetTools::GetAssetDependencyTree);
		
		// Generate input schema from USTRUCT with descriptions
		TMap<FString, FString> InputDescriptions;
		InputDescriptions.Add(TEXT("assetPath"), TEXT("The asset path to get dependency tree for. Format: '/Game/Folder/AssetName' or '/Game/Folder/AssetName.AssetName'. Examples: '/Game/MyAsset', '/Game/Blueprints/BP_Player', '/Engine/EditorMaterials/GridMaterial'. Must start with '/Game/' or '/Engine/'. Asset must exist in the project."));
		InputDescriptions.Add(TEXT("maxDepth"), TEXT("Maximum recursion depth to prevent infinite loops. Defaults to 10. Must be at least 1. Increase for deeper dependency trees, but be aware that very deep trees can be expensive to compute."));
		InputDescriptions.Add(TEXT("bIncludeHardDependencies"), TEXT("Whether to include hard dependencies (direct references). Defaults to true. Hard dependencies are assets that are directly referenced by the asset."));
		InputDescriptions.Add(TEXT("bIncludeSoftDependencies"), TEXT("Whether to include soft dependencies (searchable references). Defaults to false. Soft dependencies are assets that are referenced via searchable references (e.g., string-based asset references)."));
		TArray<FString> InputRequired;
		InputRequired.Add(TEXT("assetPath"));
		Tool.inputSchema = UMCP_GenerateJsonSchemaFromStruct<FUMCP_GetAssetDependencyTreeParams>(InputDescriptions, InputRequired);
		
		// Generate output schema from USTRUCT with descriptions
		TMap<FString, FString> OutputDescriptions;
		OutputDescriptions.Add(TEXT("bSuccess"), TEXT("Whether the operation completed successfully"));
		OutputDescriptions.Add(TEXT("assetPath"), TEXT("The asset path that was queried"));
		OutputDescriptions.Add(TEXT("tree"), TEXT("Array of dependency tree nodes, each containing assetPath, depth, and dependencies"));
		OutputDescriptions.Add(TEXT("totalNodes"), TEXT("Total number of nodes in the dependency tree"));
		OutputDescriptions.Add(TEXT("maxDepthReached"), TEXT("Maximum depth reached in the dependency tree"));
		OutputDescriptions.Add(TEXT("error"), TEXT("Error message if bSuccess is false"));
		TArray<FString> OutputRequired;
		OutputRequired.Add(TEXT("bSuccess"));
		OutputRequired.Add(TEXT("assetPath"));
		OutputRequired.Add(TEXT("tree"));
		OutputRequired.Add(TEXT("totalNodes"));
		OutputRequired.Add(TEXT("maxDepthReached"));
		TSharedPtr<FJsonObject> GetAssetDependencyTreeOutputSchema = UMCP_GenerateJsonSchemaFromStruct<FUMCP_GetAssetDependencyTreeResult>(OutputDescriptions, OutputRequired);
		if (GetAssetDependencyTreeOutputSchema.IsValid())
		{
			Tool.outputSchema = GetAssetDependencyTreeOutputSchema;
		}
		else
		{
			UE_LOG(LogUnrealMCPServer, Error, TEXT("Failed to generate outputSchema for get_asset_dependency_tree tool"));
		}
		Server->RegisterTool(MoveTemp(Tool));
	}
}

bool FUMCP_AssetTools::ExportAssetToText(const FString& ObjectPath, const FString& Format, FString& OutExportedText, FString& OutError)
{
	OutExportedText.Empty();
	OutError.Empty();

	if (ObjectPath.IsEmpty())
	{
		OutError = TEXT("ObjectPath is empty");
		return false;
	}

	// Load the object
	UObject* Object = LoadObject<UObject>(nullptr, *ObjectPath);
	if (!Object)
	{
		OutError = FString::Printf(TEXT("Failed to load Object: %s"), *ObjectPath);
		return false;
	}

	// Find exporter
	UExporter* Exporter = UExporter::FindExporter(Object, *Format);
	if (!Exporter)
	{
		OutError = FString::Printf(TEXT("Failed to find %s exporter for Object: %s"), *Format, *ObjectPath);
		return false;
	}

	// Export to text
	FStringOutputDevice OutputDevice;
	const uint32 ExportFlags = PPF_Copy | PPF_ExportsNotFullyQualified;
	UE_LOG(LogUnrealMCPServer, Verbose, TEXT("ExportAssetToText: Exporting Object '%s' to %s format using exporter: %s"), 
		*ObjectPath, *Format, *Exporter->GetClass()->GetName());
	Exporter->ExportText(nullptr, Object, *Format, OutputDevice, GWarn, ExportFlags);
	
	if (OutputDevice.IsEmpty())
	{
		OutError = FString::Printf(TEXT("ExportText did not produce any output for Object: %s. Using exporter: %s."), 
			*ObjectPath, *Exporter->GetClass()->GetName());
		return false;
	}

	OutExportedText = OutputDevice;
	return true;
}

UObject* FUMCP_AssetTools::PerformImportPass(const FString& FilePath, UClass* ImportClass, const FString& PackagePath, const FString& ObjectName)
{
	// Determine if this is a T3D file based on extension
	bool bIsT3DFile = FPaths::GetExtension(FilePath).ToLower() == TEXT("t3d");
	
	// Enable the T3D fallback factory if this is a T3D file
	if (bIsT3DFile)
	{
		UMCP_T3DFallbackFactory::SetSupportedClass(ImportClass);
	}
	
	// Find a factory that can import this file using FactoryCanImport and DoesSupportClass
	// The UFactory system will automatically determine the appropriate factory based on file type
	struct FFactoryCandidate
	{
		UFactory* Factory;
		FString ClassName;
		int32 ImportPriority;
	};
	
	TArray<FFactoryCandidate> CandidateFactories;
	
	// Collect all factories that can import this file and support the requested class
	for (UClass* Class : TObjectRange<UClass>())
	{
		if (Class->IsChildOf<UFactory>() && !Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
		{
			UFactory* TestFactory = Class->GetDefaultObject<UFactory>();
			if (TestFactory && TestFactory->FactoryCanImport(FilePath))
			{
				// Check if factory supports the requested class
				if (TestFactory->DoesSupportClass(ImportClass))
				{
					FFactoryCandidate Candidate;
					Candidate.Factory = TestFactory;
					Candidate.ClassName = Class->GetName();
					Candidate.ImportPriority = TestFactory->ImportPriority;
					CandidateFactories.Add(Candidate);
				}
			}
		}
	}
	
	// Sort by ImportPriority (higher priority first)
	CandidateFactories.Sort([](const FFactoryCandidate& A, const FFactoryCandidate& B)
	{
		return A.ImportPriority > B.ImportPriority;
	});
	
	UFactory* ImportFactory = nullptr;
	
	// Select the factory with the highest priority
	if (CandidateFactories.Num() > 0)
	{
		ImportFactory = CandidateFactories[0].Factory;
		UE_LOG(LogUnrealMCPServer, Log, TEXT("Found factory '%s' (priority: %d) that can import file: %s with class: %s"), 
			*CandidateFactories[0].ClassName, CandidateFactories[0].ImportPriority, *FilePath, *ImportClass->GetName());
	}
	
	if (!ImportFactory)
	{
		// Disable the T3D fallback factory before returning
		if (bIsT3DFile)
		{
			UMCP_T3DFallbackFactory::SetSupportedClass(nullptr);
		}
		return nullptr;
	}

	// Create or load the package
	UPackage* Package = CreatePackage(*PackagePath);
	if (!Package)
	{
		// Disable the T3D fallback factory before returning
		if (bIsT3DFile)
		{
			UMCP_T3DFallbackFactory::SetSupportedClass(nullptr);
		}
		return nullptr;
	}
	
	// Mark package as dirty
	Package->MarkPackageDirty();
	
	// Call FactoryCreateFile directly on the factory
	FName ObjectFName(*ObjectName);
	EObjectFlags ObjectFlags = RF_Public | RF_Standalone;
	bool bOperationCanceled = false;
	
	UObject* ImportedObject = ImportFactory->FactoryCreateFile(
		ImportClass,
		Package,
		ObjectFName,
		ObjectFlags,
		FilePath,
		nullptr,
		GWarn,
		bOperationCanceled
	);
	
	// Disable the T3D fallback factory after import completes
	if (bIsT3DFile)
	{
		UMCP_T3DFallbackFactory::SetSupportedClass(nullptr);
	}
	
	if (!ImportedObject || bOperationCanceled)
	{
		return nullptr;
	}
	
	return ImportedObject;
}

bool FUMCP_AssetTools::ExportAsset(TSharedPtr<FJsonObject> arguments, TArray<FUMCP_CallToolResultContent>& OutContent)
{
	auto& Content = OutContent.Add_GetRef(FUMCP_CallToolResultContent());
	Content.type = TEXT("text");

	// Convert JSON to USTRUCT at the start
	FUMCP_ExportAssetParams Params;
	if (!arguments.IsValid() || !UMCP_CreateFromJsonObject(arguments, Params))
	{
		FUMCP_ExportAssetResult ErrorResult;
		ErrorResult.bSuccess = false;
		ErrorResult.objectPath = TEXT("");
		ErrorResult.format = TEXT("");
		ErrorResult.error = TEXT("Invalid parameters");
		if (!UMCP_ToJsonString(ErrorResult, Content.text))
		{
			Content.text = TEXT("Failed to serialize error result");
		}
		return false;
	}

	// Work with USTRUCT throughout
	FUMCP_ExportAssetResult Result;
	Result.bSuccess = false;
	Result.objectPath = Params.objectPath;
	
	if (Params.objectPath.IsEmpty())
	{
		Result.format = TEXT("");
		Result.error = TEXT("Missing ObjectPath parameter.");
		if (!UMCP_ToJsonString(Result, Content.text))
		{
			Content.text = TEXT("Failed to serialize error result");
		}
		return false;
	}

	// Use default format if not specified
	if (Params.format.IsEmpty())
	{
		Params.format = TEXT("T3D");
	}
	Result.format = Params.format;

	// Check if the asset is a Blueprint - Blueprints must use batch export
	UObject* Object = LoadObject<UObject>(nullptr, *Params.objectPath);
	if (Object && Object->IsA<UBlueprint>())
	{
		Result.error = TEXT("Blueprint assets cannot be exported using export_asset. Use batch_export_assets instead, as Blueprint exports generate responses too large to be parsed.");
		if (!UMCP_ToJsonString(Result, Content.text))
		{
			Content.text = TEXT("Failed to serialize error result");
		}
		return false;
	}

	// Use helper function to export asset to text
	FString ExportedText;
	FString ExportError;
	if (!ExportAssetToText(Params.objectPath, Params.format, ExportedText, ExportError))
	{
		Result.error = ExportError;
		if (!UMCP_ToJsonString(Result, Content.text))
		{
			Content.text = TEXT("Failed to serialize error result");
		}
		return false;
	}

	// Build structured result in USTRUCT
	Result.bSuccess = true;
	Result.content = ExportedText;
	Result.error = TEXT(""); // Clear error on success

	// Convert USTRUCT to JSON string at the end
	if (!UMCP_ToJsonString(Result, Content.text))
	{
		Content.text = TEXT("Failed to serialize result");
		return false;
	}
	
	UE_LOG(LogUnrealMCPServer, Log, TEXT("ExportAsset: Successfully exported Object '%s' to %s format"), *Params.objectPath, *Params.format);
	return true;
}

bool FUMCP_AssetTools::BatchExportAssets(TSharedPtr<FJsonObject> arguments, TArray<FUMCP_CallToolResultContent>& OutContent)
{
	auto& Content = OutContent.Add_GetRef(FUMCP_CallToolResultContent());
	Content.type = TEXT("text");

	// Convert JSON to USTRUCT at the start
	FUMCP_BatchExportAssetsParams Params;
	if (!arguments.IsValid() || !UMCP_CreateFromJsonObject(arguments, Params))
	{
		FUMCP_BatchExportAssetsResult ErrorResult;
		ErrorResult.bSuccess = false;
		ErrorResult.exportedCount = 0;
		ErrorResult.failedCount = 0;
		ErrorResult.error = TEXT("Invalid parameters");
		if (!UMCP_ToJsonString(ErrorResult, Content.text))
		{
			Content.text = TEXT("Failed to serialize error result");
		}
		return false;
	}

	// Work with USTRUCT throughout
	FUMCP_BatchExportAssetsResult Result;
	Result.bSuccess = false;
	Result.exportedCount = 0;
	Result.failedCount = 0;

	if (Params.objectPaths.Num() == 0)
	{
		Result.error = TEXT("Missing or empty objectPaths parameter.");
		if (!UMCP_ToJsonString(Result, Content.text))
		{
			Content.text = TEXT("Failed to serialize error result");
		}
		return false;
	}

	if (Params.outputFolder.IsEmpty())
	{
		Result.error = TEXT("Missing outputFolder parameter.");
		if (!UMCP_ToJsonString(Result, Content.text))
		{
			Content.text = TEXT("Failed to serialize error result");
		}
		return false;
	}

	// Use default format if not specified
	if (Params.format.IsEmpty())
	{
		Params.format = TEXT("T3D");
	}

	// Convert output folder to absolute path
	FString AbsoluteOutputFolder = FPaths::ConvertRelativePathToFull(Params.outputFolder);
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	// Create output folder if it doesn't exist
	if (!PlatformFile.DirectoryExists(*AbsoluteOutputFolder))
	{
		if (!PlatformFile.CreateDirectoryTree(*AbsoluteOutputFolder))
		{
			Result.error = FString::Printf(TEXT("Failed to create output folder: %s"), *AbsoluteOutputFolder);
			if (!UMCP_ToJsonString(Result, Content.text))
			{
				Content.text = TEXT("Failed to serialize error result");
			}
			return false;
		}
		UE_LOG(LogUnrealMCPServer, Log, TEXT("Created output folder: %s"), *AbsoluteOutputFolder);
	}

	UE_LOG(LogUnrealMCPServer, Log, TEXT("BatchExportAssets: Exporting %d assets to folder: %s, format: %s"), 
		Params.objectPaths.Num(), *AbsoluteOutputFolder, *Params.format);

	// Process each asset
	for (const FString& ObjectPath : Params.objectPaths)
	{
		if (ObjectPath.IsEmpty())
		{
			Result.failedCount++;
			Result.failedPaths.Add(TEXT(""));
			UE_LOG(LogUnrealMCPServer, Warning, TEXT("BatchExportAssets: Skipping empty object path"));
			continue;
		}

		// Use helper function to export asset to text
		FString ExportedText;
		FString ExportError;
		if (!ExportAssetToText(ObjectPath, Params.format, ExportedText, ExportError))
		{
			Result.failedCount++;
			Result.failedPaths.Add(ObjectPath);
			UE_LOG(LogUnrealMCPServer, Warning, TEXT("BatchExportAssets: Failed to export Object '%s': %s"), *ObjectPath, *ExportError);
			continue;
		}

		// Generate filename from object path
		// Extract object name from path (e.g., "/Game/MyAsset.MyAsset" -> "MyAsset")
		FString ObjectName;
		int32 LastDotIndex;
		if (ObjectPath.FindLastChar(TEXT('.'), LastDotIndex))
		{
			FString PathBeforeDot = ObjectPath.Left(LastDotIndex);
			int32 LastSlashIndex;
			if (PathBeforeDot.FindLastChar(TEXT('/'), LastSlashIndex))
			{
				ObjectName = PathBeforeDot.Mid(LastSlashIndex + 1);
			}
			else
			{
				ObjectName = PathBeforeDot;
			}
		}
		else
		{
			// Fallback: use last part of path
			int32 LastSlashIndex;
			if (ObjectPath.FindLastChar(TEXT('/'), LastSlashIndex))
			{
				ObjectName = ObjectPath.Mid(LastSlashIndex + 1);
			}
			else
			{
				ObjectName = ObjectPath;
			}
		}

		// Sanitize filename (remove invalid characters)
		ObjectName = ObjectName.Replace(TEXT(" "), TEXT("_"));
		ObjectName = ObjectName.Replace(TEXT("."), TEXT("_"));
		
		// Build full file path
		FString FileName = ObjectName + TEXT(".") + Params.format.ToLower();
		FString FullFilePath = FPaths::Combine(AbsoluteOutputFolder, FileName);

		// Handle filename collisions by appending a number
		FString FinalFilePath = FullFilePath;
		int32 Counter = 1;
		while (PlatformFile.FileExists(*FinalFilePath))
		{
			FString BaseName = ObjectName;
			FString Extension = TEXT(".") + Params.format.ToLower();
			FinalFilePath = FPaths::Combine(AbsoluteOutputFolder, FString::Printf(TEXT("%s_%d%s"), *BaseName, Counter, *Extension));
			Counter++;
		}

		// Write file
		if (!FFileHelper::SaveStringToFile(ExportedText, *FinalFilePath))
		{
			Result.failedCount++;
			Result.failedPaths.Add(ObjectPath);
			UE_LOG(LogUnrealMCPServer, Warning, TEXT("BatchExportAssets: Failed to write file: %s for Object: %s"), *FinalFilePath, *ObjectPath);
			continue;
		}

		// Success
		Result.exportedCount++;
		Result.exportedPaths.Add(FinalFilePath);
		UE_LOG(LogUnrealMCPServer, Log, TEXT("BatchExportAssets: Successfully exported Object '%s' to file: %s"), *ObjectPath, *FinalFilePath);
	}

	// Overall success if at least one asset was exported
	Result.bSuccess = (Result.exportedCount > 0);
	if (!Result.bSuccess && Result.failedCount > 0)
	{
		Result.error = FString::Printf(TEXT("All %d assets failed to export"), Result.failedCount);
	}
	else if (Result.failedCount > 0)
	{
		Result.error = FString::Printf(TEXT("Partial success: %d exported, %d failed"), Result.exportedCount, Result.failedCount);
	}
	else
	{
		Result.error = TEXT("");
	}

	// Convert USTRUCT to JSON string at the end
	if (!UMCP_ToJsonString(Result, Content.text))
	{
		Content.text = TEXT("Failed to serialize result");
		return false;
	}

	UE_LOG(LogUnrealMCPServer, Log, TEXT("BatchExportAssets: Completed batch export. Exported: %d, Failed: %d"), 
		Result.exportedCount, Result.failedCount);
	
	return true;
}

bool FUMCP_AssetTools::ExportClassDefault(TSharedPtr<FJsonObject> arguments, TArray<FUMCP_CallToolResultContent>& OutContent)
{
	auto& Content = OutContent.Add_GetRef(FUMCP_CallToolResultContent());
	Content.type = TEXT("text");

	// Convert JSON to USTRUCT at the start
	FUMCP_ExportClassDefaultParams Params;
	if (!arguments.IsValid() || !UMCP_CreateFromJsonObject(arguments, Params))
	{
		FUMCP_ExportClassDefaultResult ErrorResult;
		ErrorResult.bSuccess = false;
		ErrorResult.classPath = TEXT("");
		ErrorResult.format = TEXT("");
		ErrorResult.error = TEXT("Invalid parameters");
		if (!UMCP_ToJsonString(ErrorResult, Content.text))
		{
			Content.text = TEXT("Failed to serialize error result");
		}
		return false;
	}

	// Work with USTRUCT throughout
	FUMCP_ExportClassDefaultResult Result;
	Result.bSuccess = false;
	Result.classPath = Params.classPath;
	
	if (Params.classPath.IsEmpty())
	{
		Result.format = TEXT("");
		Result.error = TEXT("Missing ClassPath parameter.");
		if (!UMCP_ToJsonString(Result, Content.text))
		{
			Content.text = TEXT("Failed to serialize error result");
		}
		return false;
	}

	// Use default format if not specified
	if (Params.format.IsEmpty())
	{
		Params.format = TEXT("T3D");
	}
	Result.format = Params.format;

	// Load the class from the class path
	UClass* Class = LoadClass<UObject>(nullptr, *Params.classPath);
	if (!Class)
	{
		Result.error = FString::Printf(TEXT("Failed to load Class: %s"), *Params.classPath);
		if (!UMCP_ToJsonString(Result, Content.text))
		{
			Content.text = TEXT("Failed to serialize error result");
		}
		return false;
	}

	// Get the class default object (CDO)
	UObject* ClassDefaultObject = Class->GetDefaultObject();
	if (!ClassDefaultObject)
	{
		Result.error = FString::Printf(TEXT("Failed to get class default object for Class: %s"), *Params.classPath);
		if (!UMCP_ToJsonString(Result, Content.text))
		{
			Content.text = TEXT("Failed to serialize error result");
		}
		return false;
	}

	UExporter* Exporter = UExporter::FindExporter(ClassDefaultObject, *Params.format);
	if (!Exporter)
	{
		Result.error = FString::Printf(TEXT("Failed to find %s exporter for Class Default Object: %s"), *Params.format, *Params.classPath);
		if (!UMCP_ToJsonString(Result, Content.text))
		{
			Content.text = TEXT("Failed to serialize error result");
		}
		return false;
	}

	FStringOutputDevice OutputDevice;
	const uint32 ExportFlags = PPF_Copy | PPF_ExportsNotFullyQualified;
	UE_LOG(LogUnrealMCPServer, Log, TEXT("Attempting to export Class Default Object for '%s' to %s format using exporter: %s"), *Params.classPath, *Params.format, *Exporter->GetClass()->GetName());
	Exporter->ExportText(nullptr, ClassDefaultObject, *Params.format, OutputDevice, GWarn, ExportFlags);
	if (OutputDevice.IsEmpty())
	{
		Result.error = FString::Printf(TEXT("ExportText did not produce any output for Class Default Object: %s. Using exporter: %s."), *Params.classPath, *Exporter->GetClass()->GetName());
		UE_LOG(LogUnrealMCPServer, Warning, TEXT("%s"), *Result.error);
		if (!UMCP_ToJsonString(Result, Content.text))
		{
			Content.text = TEXT("Failed to serialize error result");
		}
		return false;
	}

	// Build structured result in USTRUCT
	Result.bSuccess = true;
	Result.content = OutputDevice;
	Result.error = TEXT(""); // Clear error on success

	// Convert USTRUCT to JSON string at the end
	if (!UMCP_ToJsonString(Result, Content.text))
	{
		Content.text = TEXT("Failed to serialize result");
		return false;
	}
	
	UE_LOG(LogUnrealMCPServer, Log, TEXT("Successfully exported Class Default Object for '%s' to %s format"), *Params.classPath, *Params.format);
	return true;
}

bool FUMCP_AssetTools::ImportAsset(TSharedPtr<FJsonObject> arguments, TArray<FUMCP_CallToolResultContent>& OutContent)
{
	auto& Content = OutContent.Add_GetRef(FUMCP_CallToolResultContent());
	Content.type = TEXT("text");

	// Convert JSON to USTRUCT at the start
	FUMCP_ImportAssetParams Params;
	if (!arguments.IsValid() || !UMCP_CreateFromJsonObject(arguments, Params))
	{
		Content.text = TEXT("Invalid parameters");
		return false;
	}

	// Work with USTRUCT throughout
	FUMCP_ImportAssetResult Result;
	Result.bSuccess = false;
	
	// Validate that at least one file path is provided
	if (Params.filePath.IsEmpty() && Params.t3dFilePath.IsEmpty())
	{
		Result.error = TEXT("At least one of filePath or t3dFilePath must be specified.");
		
		// Convert USTRUCT to JSON string at the end
		if (!UMCP_ToJsonString(Result, Content.text))
		{
			Content.text = TEXT("Failed to serialize result");
		}
		return false;
	}
	
	if (Params.packagePath.IsEmpty())
	{
		Result.error = TEXT("Missing PackagePath parameter.");
		
		// Convert USTRUCT to JSON string at the end
		if (!UMCP_ToJsonString(Result, Content.text))
		{
			Content.text = TEXT("Failed to serialize result");
		}
		return false;
	}

	if (Params.classPath.IsEmpty())
	{
		Result.error = TEXT("Missing ClassPath parameter.");
		
		// Convert USTRUCT to JSON string at the end
		if (!UMCP_ToJsonString(Result, Content.text))
		{
			Content.text = TEXT("Failed to serialize result");
		}
		return false;
	}

	// Convert to absolute paths
	FString AbsoluteFilePath;
	FString AbsoluteT3DFilePath;
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	
	if (!Params.filePath.IsEmpty())
	{
		AbsoluteFilePath = FPaths::ConvertRelativePathToFull(Params.filePath);
		// Verify binary file exists
		if (!PlatformFile.FileExists(*AbsoluteFilePath))
		{
			Result.error = FString::Printf(TEXT("Binary file not found: %s"), *AbsoluteFilePath);
			Result.filePath = AbsoluteFilePath;
			
			// Convert USTRUCT to JSON string at the end
			if (!UMCP_ToJsonString(Result, Content.text))
			{
				Content.text = TEXT("Failed to serialize result");
			}
			return false;
		}
	}
	
	if (!Params.t3dFilePath.IsEmpty())
	{
		AbsoluteT3DFilePath = FPaths::ConvertRelativePathToFull(Params.t3dFilePath);
		// Verify T3D file exists
		if (!PlatformFile.FileExists(*AbsoluteT3DFilePath))
		{
			Result.error = FString::Printf(TEXT("T3D file not found: %s"), *AbsoluteT3DFilePath);
			Result.filePath = AbsoluteT3DFilePath;
			
			// Convert USTRUCT to JSON string at the end
			if (!UMCP_ToJsonString(Result, Content.text))
			{
				Content.text = TEXT("Failed to serialize result");
			}
			return false;
		}
	}

	// Load the class from the class path
	UClass* ImportClass = LoadClass<UObject>(nullptr, *Params.classPath);
	if (!ImportClass)
	{
		Result.error = FString::Printf(TEXT("Failed to load class: %s"), *Params.classPath);
		if (!AbsoluteFilePath.IsEmpty())
		{
			Result.filePath = AbsoluteFilePath;
		}
		else if (!AbsoluteT3DFilePath.IsEmpty())
		{
			Result.filePath = AbsoluteT3DFilePath;
		}
		
		// Convert USTRUCT to JSON string at the end
		if (!UMCP_ToJsonString(Result, Content.text))
		{
			Content.text = TEXT("Failed to serialize result");
		}
		return false;
	}

	// Parse the full object path to extract package path and object name
	// Expected format: /Game/MyFolder/MyAsset.MyAsset
	FString FullObjectPath = Params.packagePath;
	if (!FullObjectPath.StartsWith(TEXT("/")))
	{
		FullObjectPath = TEXT("/Game/") + FullObjectPath;
	}
	
	// Extract package path and object name from full object path in a single pass
	// Expected format: /Game/MyFolder/MyAsset.MyAsset
	FString PackagePath;
	FString ObjectName;
	int32 LastDotIndex;
	if (FullObjectPath.FindLastChar(TEXT('.'), LastDotIndex))
	{
		PackagePath = FullObjectPath.Left(LastDotIndex);
		ObjectName = FullObjectPath.Mid(LastDotIndex + 1);
	}
	else
	{
		// If no dot found, treat the entire path as package path (backward compatibility)
		PackagePath = FullObjectPath;
		ObjectName = FPaths::GetBaseFilename(PackagePath);
		// Append the object name to make it a full object path
		FullObjectPath = PackagePath + TEXT(".") + ObjectName;
	}

	// Check if asset already exists and validate class
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
	FSoftObjectPath SoftPath(FullObjectPath);
	FAssetData ExistingAssetData = AssetRegistry.GetAssetByObjectPath(SoftPath);
	
	if (ExistingAssetData.IsValid())
	{
		// Asset exists - validate that the class matches
		FTopLevelAssetPath ExistingClassPath = ExistingAssetData.AssetClassPath;
		FTopLevelAssetPath RequestedClassPath = ImportClass->GetClassPathName();
		
		if (ExistingClassPath != RequestedClassPath)
		{
			Result.error = FString::Printf(TEXT("Asset already exists at '%s' with class '%s', but requested class is '%s'. Cannot change asset class during import."), 
				*FullObjectPath, *ExistingClassPath.ToString(), *RequestedClassPath.ToString());
			if (!AbsoluteFilePath.IsEmpty())
			{
				Result.filePath = AbsoluteFilePath;
			}
			else if (!AbsoluteT3DFilePath.IsEmpty())
			{
				Result.filePath = AbsoluteT3DFilePath;
			}
			Result.packagePath = FullObjectPath;
			
			// Convert USTRUCT to JSON string at the end
			if (!UMCP_ToJsonString(Result, Content.text))
			{
				Content.text = TEXT("Failed to serialize result");
			}
			return false;
		}
	}

	// Try to find the target package if it exists (for use as primary object in transaction)
	UPackage* TargetPackage = FindPackage(nullptr, *PackagePath);
	if (!TargetPackage)
	{
		// Package doesn't exist yet, try loading it
		TargetPackage = LoadPackage(nullptr, *PackagePath, LOAD_None);
	}

	// Begin editor transaction to make the import undoable
	bool bTransactionStarted = false;
	if (GEditor)
	{
		GEditor->BeginTransaction(TEXT("Import Asset"), FText::FromString(TEXT("Import Asset")), TargetPackage);
		bTransactionStarted = true;
	}
	
	// Ensure transaction is always closed when scope exits
	ON_SCOPE_EXIT
	{
		if (bTransactionStarted && GEditor)
		{
			GEditor->EndTransaction();
		}
	};
	
	UObject* ImportedObject = nullptr;
	FString FactoryClassName;
	
	// Step 1: Import binary file first (if provided)
	if (!AbsoluteFilePath.IsEmpty())
	{
		ImportedObject = PerformImportPass(AbsoluteFilePath, ImportClass, PackagePath, ObjectName);
		
		if (!ImportedObject)
		{
			Result.error = FString::Printf(TEXT("Failed to import binary file: %s. Check the file format and content."), *AbsoluteFilePath);
			Result.filePath = AbsoluteFilePath;
			Result.packagePath = FullObjectPath;
			
			// Convert USTRUCT to JSON string at the end
			if (!UMCP_ToJsonString(Result, Content.text))
			{
				Content.text = TEXT("Failed to serialize result");
			}
			return false;
		}
		
		UE_LOG(LogUnrealMCPServer, Log, TEXT("Successfully imported binary file: %s"), *AbsoluteFilePath);
	}
	
	// Step 2: Import T3D file to configure the imported object (if provided)
	if (!AbsoluteT3DFilePath.IsEmpty())
	{
		// If we already imported a binary file, the object should exist
		// Otherwise, we're importing from T3D only
		if (ImportedObject == nullptr)
		{
			// Import from T3D file
			ImportedObject = PerformImportPass(AbsoluteT3DFilePath, ImportClass, PackagePath, ObjectName);
			
			if (!ImportedObject)
			{
				Result.error = FString::Printf(TEXT("Failed to import T3D file: %s. Check the file format and content."), *AbsoluteT3DFilePath);
				Result.filePath = AbsoluteT3DFilePath;
				Result.packagePath = FullObjectPath;
				
				// Convert USTRUCT to JSON string at the end
				if (!UMCP_ToJsonString(Result, Content.text))
				{
					Content.text = TEXT("Failed to serialize result");
				}
				return false;
			}
			
			UE_LOG(LogUnrealMCPServer, Log, TEXT("Successfully imported T3D file: %s"), *AbsoluteT3DFilePath);
		}
		else
		{
			// Object already exists from binary import, now apply T3D configuration
			// Re-import using T3D to configure the existing object
			UObject* T3DImportedObject = PerformImportPass(AbsoluteT3DFilePath, ImportClass, PackagePath, ObjectName);
			
			if (!T3DImportedObject)
			{
				Result.error = FString::Printf(TEXT("Failed to apply T3D configuration from file: %s. Binary import succeeded but T3D import failed."), *AbsoluteT3DFilePath);
				Result.filePath = AbsoluteT3DFilePath;
				Result.packagePath = FullObjectPath;
				
				// Convert USTRUCT to JSON string at the end
				if (!UMCP_ToJsonString(Result, Content.text))
				{
					Content.text = TEXT("Failed to serialize result");
				}
				return false;
			}
			
			// Use the T3D imported object (which should be the same object, just reconfigured)
			ImportedObject = T3DImportedObject;
			UE_LOG(LogUnrealMCPServer, Log, TEXT("Successfully applied T3D configuration from file: %s"), *AbsoluteT3DFilePath);
		}
	}
	
	// Build structured result in USTRUCT
	Result.bSuccess = true;
	Result.count = 1;
	if (!AbsoluteFilePath.IsEmpty())
	{
		Result.filePath = AbsoluteFilePath;
	}
	else if (!AbsoluteT3DFilePath.IsEmpty())
	{
		Result.filePath = AbsoluteT3DFilePath;
	}
	Result.packagePath = FullObjectPath;
	Result.factoryClass = FactoryClassName;
	
	Result.importedObjects.Empty();
	if (ImportedObject)
	{
		Result.importedObjects.Add(ImportedObject->GetPathName());
	}
	
	// Convert USTRUCT to JSON string at the end
	if (!UMCP_ToJsonString(Result, Content.text))
	{
		Content.text = TEXT("Failed to serialize result");
		return false;
	}
	
	FString ImportedFiles = TEXT("");
	if (!AbsoluteFilePath.IsEmpty())
	{
		ImportedFiles = AbsoluteFilePath;
	}
	if (!AbsoluteT3DFilePath.IsEmpty())
	{
		if (!ImportedFiles.IsEmpty())
		{
			ImportedFiles += TEXT(", ");
		}
		ImportedFiles += AbsoluteT3DFilePath;
	}
	UE_LOG(LogUnrealMCPServer, Log, TEXT("Successfully imported %d object(s) from file(s): %s"), Result.importedObjects.Num(), *ImportedFiles);
	
	return true;
}

bool FUMCP_AssetTools::QueryAsset(TSharedPtr<FJsonObject> arguments, TArray<FUMCP_CallToolResultContent>& OutContent)
{
	auto& Content = OutContent.Add_GetRef(FUMCP_CallToolResultContent());
	Content.type = TEXT("text");

	// Convert JSON to USTRUCT at the start
	FUMCP_QueryAssetParams Params;
	if (!arguments.IsValid() || !UMCP_CreateFromJsonObject(arguments, Params))
	{
		Content.text = TEXT("Invalid parameters");
		return false;
	}

	if (Params.assetPath.IsEmpty())
	{
		Content.text = TEXT("Missing required parameter: assetPath");
		return false;
	}

	UE_LOG(LogUnrealMCPServer, Log, TEXT("QueryAsset: Path=%s, IncludeTags=%s"), *Params.assetPath, Params.bIncludeTags ? TEXT("true") : TEXT("false"));

	// Get Asset Registry
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	// Use FSoftObjectPath to get asset (non-deprecated API)
	FSoftObjectPath SoftPath(Params.assetPath);
	FAssetData AssetData = AssetRegistry.GetAssetByObjectPath(SoftPath);

	// Work with USTRUCT throughout
	FUMCP_QueryAssetResult Result;
	Result.assetPath = Params.assetPath;

	if (AssetData.IsValid())
	{
		Result.bExists = true;
		Result.assetName = AssetData.AssetName.ToString();
		Result.packagePath = AssetData.PackagePath.ToString();
		Result.classPath = AssetData.AssetClassPath.ToString();
		Result.objectPath = AssetData.GetObjectPathString();
		
		if (Params.bIncludeTags)
		{
			Result.tags.Empty();
			for (const auto& TagPair : AssetData.TagsAndValues)
			{
				FString TagValue = TagPair.Value.GetValue();
				Result.tags.Add(TagPair.Key.ToString(), TagValue);
			}
		}
	}
	else
	{
		// Asset doesn't exist
		Result.bExists = false;
	}

	// Convert USTRUCT to JSON string at the end
	if (!UMCP_ToJsonString(Result, Content.text))
	{
		Content.text = TEXT("Failed to serialize result");
		return false;
	}
	
	UE_LOG(LogUnrealMCPServer, Log, TEXT("QueryAsset: Completed query for %s, exists=%s"), *Params.assetPath, Result.bExists ? TEXT("true") : TEXT("false"));
	
	return true;
}

bool FUMCP_AssetTools::SearchAssets(TSharedPtr<FJsonObject> arguments, TArray<FUMCP_CallToolResultContent>& OutContent)
{
	auto& Content = OutContent.Add_GetRef(FUMCP_CallToolResultContent());
	Content.type = TEXT("text");

	// Convert JSON to USTRUCT at the start
	FUMCP_SearchAssetsParams Params;
	if (!arguments.IsValid() || !UMCP_CreateFromJsonObject(arguments, Params))
	{
		Content.text = TEXT("Invalid parameters");
		return false;
	}

	// Work with USTRUCT throughout
	// At least one of packagePaths or packageNames must be provided
	if (Params.packagePaths.Num() == 0 && Params.packageNames.Num() == 0)
	{
		Content.text = TEXT("Missing required parameter: Either packagePaths or packageNames must be provided (at least one must be a non-empty array)");
		return false;
	}

	// Parse class paths (optional) - convert from string array to FTopLevelAssetPath
	TArray<FTopLevelAssetPath> ClassPaths;
	for (const FString& ClassPathStr : Params.classPaths)
	{
		FTopLevelAssetPath ResolvedPath = ResolveClassPath(ClassPathStr);
		if (ResolvedPath.IsValid())
		{
			ClassPaths.Add(ResolvedPath);
		}
		else
		{
			UE_LOG(LogUnrealMCPServer, Warning, TEXT("SearchAssets: Could not resolve class path: %s"), *ClassPathStr);
		}
	}

	UE_LOG(LogUnrealMCPServer, Log, TEXT("SearchAssets: PackagePaths=%d, PackageNames=%d, ClassPaths=%d, Recursive=%s, IncludeTags=%s, MaxResults=%d, Offset=%d"), 
		Params.packagePaths.Num(), Params.packageNames.Num(), ClassPaths.Num(), Params.bRecursive ? TEXT("true") : TEXT("false"), Params.bIncludeTags ? TEXT("true") : TEXT("false"), Params.maxResults, Params.offset);

	// Get Asset Registry
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	// Separate package names into exact matches and partial matches
	TArray<FString> ExactPackageNames;
	TArray<FString> PartialPackageNamePatterns;
	
	for (const FString& PackageNameStr : Params.packageNames)
	{
		if (IsPartialPackageName(PackageNameStr))
		{
			PartialPackageNamePatterns.Add(PackageNameStr);
			UE_LOG(LogUnrealMCPServer, Log, TEXT("SearchAssets: Detected partial package name pattern: %s"), *PackageNameStr);
		}
		else
		{
			ExactPackageNames.Add(PackageNameStr);
		}
	}

	// Build filter
	FARFilter Filter;
	
	// Add package paths from USTRUCT
	for (const FString& PackagePathStr : Params.packagePaths)
	{
		Filter.PackagePaths.Add(FName(*PackagePathStr));
	}
	Filter.bRecursivePaths = Params.bRecursive;
	
	// Add only exact package names to filter (partial matches will be post-filtered)
	for (const FString& PackageNameStr : ExactPackageNames)
	{
		Filter.PackageNames.Add(FName(*PackageNameStr));
	}
	
	// Add class paths if specified
	if (ClassPaths.Num() > 0)
	{
		Filter.ClassPaths = ClassPaths;
		Filter.bRecursiveClasses = true;
	}
	
	// If we have partial package name patterns but no package paths or class filters,
	// we need to require at least one to avoid expensive full asset registry searches.
	// Partial package name searches require a search scope to filter from.
	if (PartialPackageNamePatterns.Num() > 0 && Filter.PackagePaths.Num() == 0 && ClassPaths.Num() == 0 && ExactPackageNames.Num() == 0)
	{
		Content.text = TEXT("Error: Partial package name searches require either packagePaths or classPaths to define the search scope. This prevents expensive full asset registry searches. Please provide at least one package path or class filter when using partial package name patterns.");
		UE_LOG(LogUnrealMCPServer, Warning, TEXT("SearchAssets: Blocked partial package name search without package paths or class filters"));
		return false;
	}

	// Perform asset search
	TArray<FAssetData> AssetDataList;
	AssetRegistry.GetAssets(Filter, AssetDataList);

	UE_LOG(LogUnrealMCPServer, Log, TEXT("SearchAssets: Found %d assets before partial name filtering"), AssetDataList.Num());
	
	// Post-filter results for partial package name patterns
	if (PartialPackageNamePatterns.Num() > 0)
	{
		TArray<FAssetData> FilteredAssetDataList;
		
		for (const FAssetData& AssetData : AssetDataList)
		{
			// Get the full package name (package path + asset name)
			FString FullPackageName = AssetData.PackageName.ToString();
			
			// Check if this asset matches any of the partial patterns
			bool bMatchesAnyPattern = false;
			for (const FString& Pattern : PartialPackageNamePatterns)
			{
				if (MatchesPackageNamePattern(FullPackageName, Pattern))
				{
					bMatchesAnyPattern = true;
					break;
				}
			}
			
			if (bMatchesAnyPattern)
			{
				FilteredAssetDataList.Add(AssetData);
			}
		}
		
		AssetDataList = MoveTemp(FilteredAssetDataList);
		UE_LOG(LogUnrealMCPServer, Log, TEXT("SearchAssets: Found %d assets after partial name filtering"), AssetDataList.Num());
	}

	// Apply paging: calculate total count before limiting
	int32 TotalCount = AssetDataList.Num();
	int32 StartIndex = FMath::Max(0, Params.offset);
	int32 EndIndex = AssetDataList.Num();
	
	// Apply maxResults limit if specified
	if (Params.maxResults > 0)
	{
		EndIndex = FMath::Min(StartIndex + Params.maxResults, AssetDataList.Num());
	}
	
	// Extract the page of results
	TArray<FAssetData> PagedAssetDataList;
	if (StartIndex < AssetDataList.Num())
	{
		for (int32 i = StartIndex; i < EndIndex; ++i)
		{
			PagedAssetDataList.Add(AssetDataList[i]);
		}
	}
	
	UE_LOG(LogUnrealMCPServer, Log, TEXT("SearchAssets: Returning %d assets (offset=%d, maxResults=%d, total=%d)"), 
		PagedAssetDataList.Num(), Params.offset, Params.maxResults, TotalCount);

	// Build results - output has complex nested structure, so we build JSON manually
	// but we've used USTRUCT for all input processing
	TSharedPtr<FJsonObject> ResultsJson = MakeShareable(new FJsonObject);
	TArray<TSharedPtr<FJsonValue>> AssetsArray;

	for (const FAssetData& AssetData : PagedAssetDataList)
	{
		TSharedPtr<FJsonObject> AssetJson = MakeShareable(new FJsonObject);
		AssetDataToJson(AssetData, AssetJson, Params.bIncludeTags);
		AssetsArray.Add(MakeShareable(new FJsonValueObject(AssetJson)));
	}

	ResultsJson->SetArrayField(TEXT("assets"), AssetsArray);
	ResultsJson->SetNumberField(TEXT("count"), AssetsArray.Num());
	ResultsJson->SetNumberField(TEXT("totalCount"), TotalCount);
	ResultsJson->SetNumberField(TEXT("offset"), Params.offset);
	ResultsJson->SetBoolField(TEXT("hasMore"), EndIndex < TotalCount);

	// Convert to JSON string
	FString ResultJsonString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResultJsonString);
	FJsonSerializer::Serialize(ResultsJson.ToSharedRef(), Writer);

	Content.text = ResultJsonString;
	
	UE_LOG(LogUnrealMCPServer, Log, TEXT("SearchAssets: Completed search, found %d assets"), AssetsArray.Num());
	
	return true;
}

bool FUMCP_AssetTools::GetAssetDependencies(TSharedPtr<FJsonObject> arguments, TArray<FUMCP_CallToolResultContent>& OutContent)
{
	auto& Content = OutContent.Add_GetRef(FUMCP_CallToolResultContent());
	Content.type = TEXT("text");

	// Convert JSON to USTRUCT at the start
	FUMCP_GetAssetDependenciesParams Params;
	if (!arguments.IsValid() || !UMCP_CreateFromJsonObject(arguments, Params))
	{
		Content.text = TEXT("Invalid parameters");
		return false;
	}

	if (Params.assetPath.IsEmpty())
	{
		Content.text = TEXT("Missing required parameter: assetPath");
		return false;
	}

	UE_LOG(LogUnrealMCPServer, Log, TEXT("GetAssetDependencies: Path=%s, Hard=%s, Soft=%s"), 
		*Params.assetPath, Params.bIncludeHardDependencies ? TEXT("true") : TEXT("false"), 
		Params.bIncludeSoftDependencies ? TEXT("true") : TEXT("false"));

	// Get Asset Registry
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	// Use FSoftObjectPath to get asset
	FSoftObjectPath SoftPath(Params.assetPath);
	FAssetData AssetData = AssetRegistry.GetAssetByObjectPath(SoftPath);

	// Work with USTRUCT throughout
	FUMCP_GetAssetDependenciesResult Result;
	Result.assetPath = Params.assetPath;
	Result.bSuccess = false;
	Result.count = 0;

	if (!AssetData.IsValid())
	{
		Result.error = FString::Printf(TEXT("Asset not found: %s"), *Params.assetPath);
		if (!UMCP_ToJsonString(Result, Content.text))
		{
			Content.text = TEXT("Failed to serialize error result");
		}
		return false;
	}

	// Build dependency query flags using new API
	// Check if neither dependency type is requested
	if (!Params.bIncludeHardDependencies && !Params.bIncludeSoftDependencies)
	{
		// Neither hard nor soft dependencies requested - return empty result
		Result.count = 0;
		Result.bSuccess = true;
		if (!UMCP_ToJsonString(Result, Content.text))
		{
			Content.text = TEXT("Failed to serialize result");
		}
		return true;
	}

	// Build dependency query flags
	UE::AssetRegistry::EDependencyQuery DependencyQueryEnum;
	if (Params.bIncludeHardDependencies && Params.bIncludeSoftDependencies)
	{
		DependencyQueryEnum = UE::AssetRegistry::EDependencyQuery::Hard | UE::AssetRegistry::EDependencyQuery::Soft;
	}
	else if (Params.bIncludeHardDependencies)
	{
		DependencyQueryEnum = UE::AssetRegistry::EDependencyQuery::Hard;
	}
	else // Params.bIncludeSoftDependencies must be true
	{
		DependencyQueryEnum = UE::AssetRegistry::EDependencyQuery::Soft;
	}

	// Get dependencies
	TArray<FAssetIdentifier> Dependencies;
	UE::AssetRegistry::FDependencyQuery DependencyQuery(DependencyQueryEnum);
	AssetRegistry.GetDependencies(FAssetIdentifier(AssetData.PackageName), Dependencies, UE::AssetRegistry::EDependencyCategory::Package, DependencyQuery);

	// Convert to string array
	Result.dependencies.Empty();
	for (const FAssetIdentifier& Dep : Dependencies)
	{
		// Get asset data for the dependency
		TArray<FAssetData> DepAssetDataArray;
		AssetRegistry.GetAssetsByPackageName(Dep.PackageName, DepAssetDataArray);
		if (DepAssetDataArray.Num() > 0 && DepAssetDataArray[0].IsValid())
		{
			Result.dependencies.Add(DepAssetDataArray[0].GetSoftObjectPath().ToString());
		}
		else
		{
			// If we can't get asset data, use package name as fallback
			Result.dependencies.Add(Dep.PackageName.ToString());
		}
	}

	Result.count = Result.dependencies.Num();
	Result.bSuccess = true;

	// Convert USTRUCT to JSON string at the end
	if (!UMCP_ToJsonString(Result, Content.text))
	{
		Content.text = TEXT("Failed to serialize result");
		return false;
	}

	UE_LOG(LogUnrealMCPServer, Log, TEXT("GetAssetDependencies: Completed for %s, found %d dependencies"), 
		*Params.assetPath, Result.count);

	return true;
}

bool FUMCP_AssetTools::GetAssetReferences(TSharedPtr<FJsonObject> arguments, TArray<FUMCP_CallToolResultContent>& OutContent)
{
	auto& Content = OutContent.Add_GetRef(FUMCP_CallToolResultContent());
	Content.type = TEXT("text");

	// Convert JSON to USTRUCT at the start
	FUMCP_GetAssetReferencesParams Params;
	if (!arguments.IsValid() || !UMCP_CreateFromJsonObject(arguments, Params))
	{
		Content.text = TEXT("Invalid parameters");
		return false;
	}

	if (Params.assetPath.IsEmpty())
	{
		Content.text = TEXT("Missing required parameter: assetPath");
		return false;
	}

	UE_LOG(LogUnrealMCPServer, Log, TEXT("GetAssetReferences: Path=%s, Hard=%s, Soft=%s"), 
		*Params.assetPath, Params.bIncludeHardReferences ? TEXT("true") : TEXT("false"), 
		Params.bIncludeSoftReferences ? TEXT("true") : TEXT("false"));

	// Get Asset Registry
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	// Use FSoftObjectPath to get asset
	FSoftObjectPath SoftPath(Params.assetPath);
	FAssetData AssetData = AssetRegistry.GetAssetByObjectPath(SoftPath);

	// Work with USTRUCT throughout
	FUMCP_GetAssetReferencesResult Result;
	Result.assetPath = Params.assetPath;
	Result.bSuccess = false;
	Result.count = 0;

	if (!AssetData.IsValid())
	{
		Result.error = FString::Printf(TEXT("Asset not found: %s"), *Params.assetPath);
		if (!UMCP_ToJsonString(Result, Content.text))
		{
			Content.text = TEXT("Failed to serialize error result");
		}
		return false;
	}

	// Build dependency query flags using new API (same enum used for references)
	// Check if neither reference type is requested
	if (!Params.bIncludeHardReferences && !Params.bIncludeSoftReferences)
	{
		// Neither hard nor soft references requested - return empty result
		Result.count = 0;
		Result.bSuccess = true;
		if (!UMCP_ToJsonString(Result, Content.text))
		{
			Content.text = TEXT("Failed to serialize result");
		}
		return true;
	}

	// Build dependency query flags
	UE::AssetRegistry::EDependencyQuery DependencyQueryEnum;
	if (Params.bIncludeHardReferences && Params.bIncludeSoftReferences)
	{
		DependencyQueryEnum = UE::AssetRegistry::EDependencyQuery::Hard | UE::AssetRegistry::EDependencyQuery::Soft;
	}
	else if (Params.bIncludeHardReferences)
	{
		DependencyQueryEnum = UE::AssetRegistry::EDependencyQuery::Hard;
	}
	else // Params.bIncludeSoftReferences must be true
	{
		DependencyQueryEnum = UE::AssetRegistry::EDependencyQuery::Soft;
	}

	// Get referencers (assets that reference this asset)
	TArray<FAssetIdentifier> Referencers;
	UE::AssetRegistry::FDependencyQuery DependencyQuery(DependencyQueryEnum);
	AssetRegistry.GetReferencers(FAssetIdentifier(AssetData.PackageName), Referencers, UE::AssetRegistry::EDependencyCategory::Package, DependencyQuery);

	// Convert to string array
	Result.references.Empty();
	for (const FAssetIdentifier& Ref : Referencers)
	{
		// Get asset data for the referencer
		TArray<FAssetData> RefAssetDataArray;
		AssetRegistry.GetAssetsByPackageName(Ref.PackageName, RefAssetDataArray);
		if (RefAssetDataArray.Num() > 0 && RefAssetDataArray[0].IsValid())
		{
			Result.references.Add(RefAssetDataArray[0].GetSoftObjectPath().ToString());
		}
		else
		{
			// If we can't get asset data, use package name as fallback
			Result.references.Add(Ref.PackageName.ToString());
		}
	}

	Result.count = Result.references.Num();
	Result.bSuccess = true;

	// Convert USTRUCT to JSON string at the end
	if (!UMCP_ToJsonString(Result, Content.text))
	{
		Content.text = TEXT("Failed to serialize result");
		return false;
	}

	UE_LOG(LogUnrealMCPServer, Log, TEXT("GetAssetReferences: Completed for %s, found %d references"), 
		*Params.assetPath, Result.count);

	return true;
}

bool FUMCP_AssetTools::GetAssetDependencyTree(TSharedPtr<FJsonObject> arguments, TArray<FUMCP_CallToolResultContent>& OutContent)
{
	auto& Content = OutContent.Add_GetRef(FUMCP_CallToolResultContent());
	Content.type = TEXT("text");

	// Convert JSON to USTRUCT at the start
	FUMCP_GetAssetDependencyTreeParams Params;
	if (!arguments.IsValid() || !UMCP_CreateFromJsonObject(arguments, Params))
	{
		Content.text = TEXT("Invalid parameters");
		return false;
	}

	if (Params.assetPath.IsEmpty())
	{
		Content.text = TEXT("Missing required parameter: assetPath");
		return false;
	}

	if (Params.maxDepth < 1)
	{
		Content.text = TEXT("maxDepth must be at least 1");
		return false;
	}

	UE_LOG(LogUnrealMCPServer, Log, TEXT("GetAssetDependencyTree: Path=%s, MaxDepth=%d, Hard=%s, Soft=%s"), 
		*Params.assetPath, Params.maxDepth, Params.bIncludeHardDependencies ? TEXT("true") : TEXT("false"), 
		Params.bIncludeSoftDependencies ? TEXT("true") : TEXT("false"));

	// Get Asset Registry
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	// Use FSoftObjectPath to get asset
	FSoftObjectPath SoftPath(Params.assetPath);
	FAssetData AssetData = AssetRegistry.GetAssetByObjectPath(SoftPath);

	// Work with USTRUCT throughout
	FUMCP_GetAssetDependencyTreeResult Result;
	Result.assetPath = Params.assetPath;
	Result.bSuccess = false;
	Result.totalNodes = 0;
	Result.maxDepthReached = 0;

	if (!AssetData.IsValid())
	{
		Result.error = FString::Printf(TEXT("Asset not found: %s"), *Params.assetPath);
		if (!UMCP_ToJsonString(Result, Content.text))
		{
			Content.text = TEXT("Failed to serialize error result");
		}
		return false;
	}

	// Build dependency query flags using new API
	// Check if neither dependency type is requested
	if (!Params.bIncludeHardDependencies && !Params.bIncludeSoftDependencies)
	{
		// Neither hard nor soft dependencies requested - return empty tree
		Result.bSuccess = true;
		Result.totalNodes = 1; // Just the root node
		Result.maxDepthReached = 0;
		FUMCP_AssetDependencyNode RootNode;
		RootNode.assetPath = Params.assetPath;
		RootNode.depth = 0;
		RootNode.dependencies.Empty();
		Result.tree.Add(RootNode);
		if (!UMCP_ToJsonString(Result, Content.text))
		{
			Content.text = TEXT("Failed to serialize result");
		}
		return true;
	}

	// Build dependency query flags
	UE::AssetRegistry::EDependencyQuery DependencyQueryEnum;
	if (Params.bIncludeHardDependencies && Params.bIncludeSoftDependencies)
	{
		DependencyQueryEnum = UE::AssetRegistry::EDependencyQuery::Hard | UE::AssetRegistry::EDependencyQuery::Soft;
	}
	else if (Params.bIncludeHardDependencies)
	{
		DependencyQueryEnum = UE::AssetRegistry::EDependencyQuery::Hard;
	}
	else // Params.bIncludeSoftDependencies must be true
	{
		DependencyQueryEnum = UE::AssetRegistry::EDependencyQuery::Soft;
	}

	// Recursive function to build dependency tree
	TSet<FName> VisitedPackages; // Track visited packages to prevent cycles
	TMap<FName, FString> PackageToAssetPath; // Cache package name to asset path mapping
	UE::AssetRegistry::FDependencyQuery DependencyQuery(DependencyQueryEnum);

	// Use TFunction to allow recursive lambda
	TFunction<void(const FAssetIdentifier&, int32, FUMCP_AssetDependencyNode&)> BuildDependencyTreeRecursive;
	BuildDependencyTreeRecursive = [&](const FAssetIdentifier& AssetId, int32 CurrentDepth, FUMCP_AssetDependencyNode& OutNode) -> void
	{
		if (CurrentDepth > Params.maxDepth)
		{
			return;
		}

		// Get asset path for this node
		FString AssetPathStr;
		if (PackageToAssetPath.Contains(AssetId.PackageName))
		{
			AssetPathStr = PackageToAssetPath[AssetId.PackageName];
		}
		else
		{
			TArray<FAssetData> NodeAssetDataArray;
			AssetRegistry.GetAssetsByPackageName(AssetId.PackageName, NodeAssetDataArray);
			if (NodeAssetDataArray.Num() > 0 && NodeAssetDataArray[0].IsValid())
			{
				AssetPathStr = NodeAssetDataArray[0].GetSoftObjectPath().ToString();
				PackageToAssetPath.Add(AssetId.PackageName, AssetPathStr);
			}
			else
			{
				AssetPathStr = AssetId.PackageName.ToString();
			}
		}

		OutNode.assetPath = AssetPathStr;
		OutNode.depth = CurrentDepth;
		OutNode.dependencies.Empty();

		// Mark as visited to prevent cycles
		VisitedPackages.Add(AssetId.PackageName);

		// Get dependencies for this asset
		TArray<FAssetIdentifier> Dependencies;
		AssetRegistry.GetDependencies(AssetId, Dependencies, UE::AssetRegistry::EDependencyCategory::Package, DependencyQuery);

		// Process each dependency
		for (const FAssetIdentifier& Dep : Dependencies)
		{
			// Get asset path for dependency
			FString DepAssetPathStr;
			if (PackageToAssetPath.Contains(Dep.PackageName))
			{
				DepAssetPathStr = PackageToAssetPath[Dep.PackageName];
			}
			else
			{
				TArray<FAssetData> DepAssetDataArray;
				AssetRegistry.GetAssetsByPackageName(Dep.PackageName, DepAssetDataArray);
				if (DepAssetDataArray.Num() > 0 && DepAssetDataArray[0].IsValid())
				{
					DepAssetPathStr = DepAssetDataArray[0].GetSoftObjectPath().ToString();
					PackageToAssetPath.Add(Dep.PackageName, DepAssetPathStr);
				}
				else
				{
					DepAssetPathStr = Dep.PackageName.ToString();
				}
			}

			OutNode.dependencies.Add(DepAssetPathStr);

			// Recursively process dependency if not visited and within depth limit
			if (!VisitedPackages.Contains(Dep.PackageName) && CurrentDepth < Params.maxDepth)
			{
				FUMCP_AssetDependencyNode ChildNode;
				BuildDependencyTreeRecursive(Dep, CurrentDepth + 1, ChildNode);
				if (!ChildNode.assetPath.IsEmpty())
				{
					Result.tree.Add(ChildNode);
				}
			}
		}

		// Unmark as visited (allow it to appear in different branches)
		VisitedPackages.Remove(AssetId.PackageName);
	};

	// Build tree starting from root asset
	FUMCP_AssetDependencyNode RootNode;
	BuildDependencyTreeRecursive(FAssetIdentifier(AssetData.PackageName), 0, RootNode);
	Result.tree.Insert(RootNode, 0); // Insert root at beginning

	// Calculate statistics
	Result.totalNodes = Result.tree.Num();
	for (const FUMCP_AssetDependencyNode& Node : Result.tree)
	{
		if (Node.depth > Result.maxDepthReached)
		{
			Result.maxDepthReached = Node.depth;
		}
	}

	Result.bSuccess = true;

	// Convert USTRUCT to JSON string at the end
	if (!UMCP_ToJsonString(Result, Content.text))
	{
		Content.text = TEXT("Failed to serialize result");
		return false;
	}

	UE_LOG(LogUnrealMCPServer, Log, TEXT("GetAssetDependencyTree: Completed for %s, found %d nodes, max depth=%d"), 
		*Params.assetPath, Result.totalNodes, Result.maxDepthReached);

	return true;
}

