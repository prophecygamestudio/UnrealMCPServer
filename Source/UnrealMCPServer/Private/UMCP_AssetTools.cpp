#include "UMCP_AssetTools.h"
#include "UMCP_Server.h"
#include "UMCP_Types.h"
#include "UMCP_T3DFallbackFactory.h"
#include "UnrealMCPServerModule.h"
#include "UObject/UnrealType.h"
#include "Exporters/Exporter.h"
#include "AssetRegistry/AssetRegistryModule.h"
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


namespace
{
	TSharedPtr<FJsonObject> FromJsonStr(const FString& Str)
	{
		TSharedPtr<FJsonObject> RootJsonObject;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Str);

		if (!FJsonSerializer::Deserialize(Reader, RootJsonObject) || !RootJsonObject.IsValid())
		{
			UE_LOG(LogTemp, Error, TEXT("FJsonRpcRequest::CreateFromJsonString: Failed to deserialize JsonString. String: %s"), *Str);
			return nullptr;
		}

		return RootJsonObject;
	}

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
}


void FUMCP_AssetTools::Register(class FUMCP_Server* Server)
{
	{
		FUMCP_ToolDefinition Tool;
		Tool.name = TEXT("search_blueprints");
		Tool.description = TEXT("Search for Blueprint assets based on various criteria including name patterns, parent classes, and package paths.");
		Tool.DoToolCall.BindRaw(this, &FUMCP_AssetTools::SearchBlueprints);
		
		// Generate input schema from USTRUCT with descriptions and enum
		TMap<FString, FString> InputDescriptions;
		InputDescriptions.Add(TEXT("searchType"), TEXT("Type of search to perform: 'name' for name pattern matching, 'parent_class' for finding Blueprint subclasses, 'all' for comprehensive search"));
		InputDescriptions.Add(TEXT("searchTerm"), TEXT("Search term (Blueprint name pattern, parent class name, etc.)."));
		InputDescriptions.Add(TEXT("packagePath"), TEXT("Optional package path to limit search scope (e.g., '/Game/Blueprints'). If not specified, searches entire project."));
		InputDescriptions.Add(TEXT("recursive"), TEXT("Whether to search recursively in subfolders. Defaults to true."));
		TArray<FString> InputRequired;
		InputRequired.Add(TEXT("searchType"));
		InputRequired.Add(TEXT("searchTerm"));
		TMap<FString, TArray<FString>> EnumValues;
		EnumValues.Add(TEXT("searchType"), { TEXT("name"), TEXT("parent_class"), TEXT("all") });
		Tool.inputSchema = UMCP_GenerateJsonSchemaFromStruct(FUMCP_SearchBlueprintsParams::StaticStruct(), InputDescriptions, InputRequired, EnumValues);
		
		// Output schema is complex with nested objects, so we'll keep it as manual JSON for now
		// TODO: Could create USTRUCT for output and generate schema, but nested match objects are complex
		FString SearchOutputSchema = TEXT("{")
			TEXT("\"type\":\"object\",")
			TEXT("\"properties\":{")
			TEXT("\"results\":{\"type\":\"array\",\"description\":\"Array of matching Blueprint assets\",\"items\":{\"type\":\"object\",\"properties\":{")
			TEXT("\"assetPath\":{\"type\":\"string\"},")
			TEXT("\"assetName\":{\"type\":\"string\"},")
			TEXT("\"packagePath\":{\"type\":\"string\"},")
			TEXT("\"parentClass\":{\"type\":\"string\"},")
			TEXT("\"matches\":{\"type\":\"array\",\"items\":{\"type\":\"object\"}}")
			TEXT("}}},")
			TEXT("\"totalResults\":{\"type\":\"number\",\"description\":\"Total number of matching results\"},")
			TEXT("\"searchCriteria\":{\"type\":\"object\",\"description\":\"The search criteria used\",\"properties\":{")
			TEXT("\"searchType\":{\"type\":\"string\"},")
			TEXT("\"searchTerm\":{\"type\":\"string\"},")
			TEXT("\"packagePath\":{\"type\":\"string\",\"description\":\"Optional package path if specified\"},")
			TEXT("\"recursive\":{\"type\":\"boolean\"}")
			TEXT("},\"required\":[\"searchType\",\"searchTerm\",\"recursive\"]")
			TEXT("}},")
			TEXT("\"required\":[\"results\",\"totalResults\",\"searchCriteria\"]")
			TEXT("}");
		TSharedPtr<FJsonObject> ParsedSchema = FromJsonStr(SearchOutputSchema);
		if (ParsedSchema.IsValid())
		{
			Tool.outputSchema = ParsedSchema;
		}
		else
		{
			UE_LOG(LogUnrealMCPServer, Error, TEXT("Failed to parse outputSchema for search_blueprints tool"));
		}
		Server->RegisterTool(MoveTemp(Tool));
	}
	
	{
		FUMCP_ToolDefinition Tool;
		Tool.name = TEXT("export_asset");
		Tool.description = TEXT("Export a single UObject to a specified format (defaults to T3D). Returns the exported content as a string.");
		Tool.DoToolCall.BindRaw(this, &FUMCP_AssetTools::ExportAsset);
		
		// Generate input schema from USTRUCT with descriptions
		TMap<FString, FString> InputDescriptions;
		InputDescriptions.Add(TEXT("objectPath"), TEXT("The path to the object to export"));
		InputDescriptions.Add(TEXT("format"), TEXT("The export format (e.g., 'T3D'). Defaults to 'T3D' if not specified."));
		TArray<FString> InputRequired;
		InputRequired.Add(TEXT("objectPath"));
		Tool.inputSchema = UMCP_GenerateJsonSchemaFromStruct(FUMCP_ExportAssetParams::StaticStruct(), InputDescriptions, InputRequired);
		
		// Generate output schema from USTRUCT with descriptions
		TMap<FString, FString> OutputDescriptions;
		OutputDescriptions.Add(TEXT("bSuccess"), TEXT("Whether the export was successful"));
		OutputDescriptions.Add(TEXT("objectPath"), TEXT("The path to the object that was exported"));
		OutputDescriptions.Add(TEXT("format"), TEXT("The export format used (e.g., 'T3D', 'OBJ')"));
		OutputDescriptions.Add(TEXT("content"), TEXT("The exported asset content in the specified format. The format varies depending on the exporter type and object type."));
		OutputDescriptions.Add(TEXT("error"), TEXT("Error message if bSuccess is false"));
		TArray<FString> OutputRequired;
		OutputRequired.Add(TEXT("bSuccess"));
		OutputRequired.Add(TEXT("objectPath"));
		TSharedPtr<FJsonObject> ExportOutputSchema = UMCP_GenerateJsonSchemaFromStruct(FUMCP_ExportAssetResult::StaticStruct(), OutputDescriptions, OutputRequired);
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
		Tool.description = TEXT("Export multiple assets to files in a specified folder. Returns a list of the exported file paths.");
		Tool.DoToolCall.BindRaw(this, &FUMCP_AssetTools::BatchExportAssets);
		
		// Generate input schema from USTRUCT with descriptions
		TMap<FString, FString> InputDescriptions;
		InputDescriptions.Add(TEXT("objectPaths"), TEXT("Array of object paths to export"));
		InputDescriptions.Add(TEXT("outputFolder"), TEXT("The folder path where exported files should be saved"));
		InputDescriptions.Add(TEXT("format"), TEXT("The export format (e.g., 'T3D'). Defaults to 'T3D' if not specified."));
		TArray<FString> InputRequired;
		InputRequired.Add(TEXT("objectPaths"));
		InputRequired.Add(TEXT("outputFolder"));
		Tool.inputSchema = UMCP_GenerateJsonSchemaFromStruct(FUMCP_BatchExportAssetsParams::StaticStruct(), InputDescriptions, InputRequired);
		
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
		TSharedPtr<FJsonObject> BatchExportOutputSchema = UMCP_GenerateJsonSchemaFromStruct(FUMCP_BatchExportAssetsResult::StaticStruct(), OutputDescriptions, OutputRequired);
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
		Tool.description = TEXT("Export the class default object for a given class path. This allows determining default values for a class, since exporting instances of objects do not print values that are identical to the default value.");
		Tool.DoToolCall.BindRaw(this, &FUMCP_AssetTools::ExportClassDefault);
		
		// Generate input schema from USTRUCT with descriptions
		TMap<FString, FString> InputDescriptions;
		InputDescriptions.Add(TEXT("classPath"), TEXT("The class path to export the default object for (e.g., '/Script/Engine.Actor')"));
		InputDescriptions.Add(TEXT("format"), TEXT("The export format (e.g., 'T3D'). Defaults to 'T3D' if not specified."));
		TArray<FString> InputRequired;
		InputRequired.Add(TEXT("classPath"));
		Tool.inputSchema = UMCP_GenerateJsonSchemaFromStruct(FUMCP_ExportClassDefaultParams::StaticStruct(), InputDescriptions, InputRequired);
		
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
		TSharedPtr<FJsonObject> ExportClassDefaultOutputSchema = UMCP_GenerateJsonSchemaFromStruct(FUMCP_ExportClassDefaultResult::StaticStruct(), OutputDescriptions, OutputRequired);
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
		Tool.description = TEXT("Import a file to create or update a UObject. The file type is automatically detected based on available factories.");
		Tool.DoToolCall.BindRaw(this, &FUMCP_AssetTools::ImportAsset);
		
		// Generate input schema from USTRUCT with descriptions
		TMap<FString, FString> InputDescriptions;
		InputDescriptions.Add(TEXT("filePath"), TEXT("The path to the binary file to import (e.g., texture, mesh). Optional if t3dFilePath is provided. At least one of filePath or t3dFilePath must be specified."));
		InputDescriptions.Add(TEXT("t3dFilePath"), TEXT("The path to the T3D file for configuration. Optional if filePath is provided. At least one of filePath or t3dFilePath must be specified."));
		InputDescriptions.Add(TEXT("packagePath"), TEXT("The full object path where the object should be created, including the object name (e.g., '/Game/MyFolder/MyAsset.MyAsset')"));
		InputDescriptions.Add(TEXT("classPath"), TEXT("The class path of the object to import (e.g., '/Script/Engine.Texture2D')"));
		TArray<FString> InputRequired;
		InputRequired.Add(TEXT("packagePath"));
		InputRequired.Add(TEXT("classPath"));
		Tool.inputSchema = UMCP_GenerateJsonSchemaFromStruct(FUMCP_ImportAssetParams::StaticStruct(), InputDescriptions, InputRequired);
		
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
		TSharedPtr<FJsonObject> ImportOutputSchema = UMCP_GenerateJsonSchemaFromStruct(FUMCP_ImportAssetResult::StaticStruct(), OutputDescriptions, OutputRequired);
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
		Tool.description = TEXT("Query a single asset to check if it exists and get its basic information from the asset registry.");
		Tool.DoToolCall.BindRaw(this, &FUMCP_AssetTools::QueryAsset);
		
		// Generate input schema from USTRUCT with descriptions
		TMap<FString, FString> InputDescriptions;
		InputDescriptions.Add(TEXT("assetPath"), TEXT("Single asset path to query (e.g., '/Game/MyAsset' or '/Game/MyFolder/MyAsset')"));
		InputDescriptions.Add(TEXT("bIncludeTags"), TEXT("Whether to include asset tags in the response. Defaults to false."));
		TArray<FString> InputRequired;
		InputRequired.Add(TEXT("assetPath"));
		Tool.inputSchema = UMCP_GenerateJsonSchemaFromStruct(FUMCP_QueryAssetParams::StaticStruct(), InputDescriptions, InputRequired);
		
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
		TSharedPtr<FJsonObject> QueryOutputSchema = UMCP_GenerateJsonSchemaFromStruct(FUMCP_QueryAssetResult::StaticStruct(), OutputDescriptions, OutputRequired);
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
		Tool.description = TEXT("Search for assets by package paths or package names, optionally filtered by class. Returns an array of asset information from the asset registry.");
		Tool.DoToolCall.BindRaw(this, &FUMCP_AssetTools::SearchAssets);
		
		// Generate input schema from USTRUCT with descriptions
		TMap<FString, FString> InputDescriptions;
		InputDescriptions.Add(TEXT("packagePaths"), TEXT("Array of directory/package paths to search for assets (e.g., ['/Game/Blueprints', '/Game/Materials']). Optional if packageNames is provided."));
		InputDescriptions.Add(TEXT("packageNames"), TEXT("Array of full package names to search for (e.g., ['MyAsset', '/Game/MyAsset']). Must be exact full package names - partial matches are not supported. Can be used instead of or in combination with packagePaths."));
		InputDescriptions.Add(TEXT("classPaths"), TEXT("Array of class paths to filter by (e.g., ['/Script/Engine.Blueprint', '/Script/Engine.Texture2D']). If empty, searches all asset types."));
		InputDescriptions.Add(TEXT("bRecursive"), TEXT("Whether to search recursively in subdirectories. Defaults to true."));
		InputDescriptions.Add(TEXT("bIncludeTags"), TEXT("Whether to include asset tags in the response. Defaults to false."));
		TArray<FString> InputRequired;
		// packagePaths is required only if packageNames is empty
		Tool.inputSchema = UMCP_GenerateJsonSchemaFromStruct(FUMCP_SearchAssetsParams::StaticStruct(), InputDescriptions, InputRequired);
		
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
		TSharedPtr<FJsonObject> ParsedSchema = FromJsonStr(SearchAssetsOutputSchema);
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
}

bool FUMCP_AssetTools::SearchBlueprints(TSharedPtr<FJsonObject> arguments, TArray<FUMCP_CallToolResultContent>& OutContent)
{
	auto& Content = OutContent.Add_GetRef(FUMCP_CallToolResultContent());
	Content.type = TEXT("text");

	// Convert JSON to USTRUCT at the start
	FUMCP_SearchBlueprintsParams Params;
	if (!arguments.IsValid() || !UMCP_CreateFromJsonObject(arguments, Params))
	{
		Content.text = TEXT("Invalid parameters");
		return false;
	}

	// Work with USTRUCT throughout
	// Validate required parameters
	if (Params.searchType.IsEmpty() || Params.searchTerm.IsEmpty())
	{
		Content.text = TEXT("Missing required parameters: searchType and searchTerm are required.");
		return false;
	}

	UE_LOG(LogUnrealMCPServer, Log, TEXT("SearchBlueprints: Type=%s, Term=%s, Path=%s, Recursive=%s"), 
		*Params.searchType, *Params.searchTerm, *Params.packagePath, Params.bRecursive ? TEXT("true") : TEXT("false"));

	// Get Asset Registry
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	// Prepare search filter
	FARFilter Filter;
	Filter.ClassPaths.Add(UBlueprint::StaticClass()->GetClassPathName());
	Filter.bRecursiveClasses = true;
	
	// Add package path filter if specified
	if (!Params.packagePath.IsEmpty())
	{
		Filter.PackagePaths.Add(FName(*Params.packagePath));
		Filter.bRecursivePaths = Params.bRecursive;
	}

	// Perform asset search
	TArray<FAssetData> AssetDataList;
	AssetRegistry.GetAssets(Filter, AssetDataList);

	UE_LOG(LogUnrealMCPServer, Log, TEXT("SearchBlueprints: Found %d Blueprint assets before filtering"), AssetDataList.Num());

	// Build results JSON
	TSharedPtr<FJsonObject> ResultsJson = MakeShareable(new FJsonObject);
	TArray<TSharedPtr<FJsonValue>> ResultsArray;
	int32 TotalMatches = 0;

	for (const FAssetData& AssetData : AssetDataList)
	{
		bool bMatches = false;
		TArray<TSharedPtr<FJsonValue>> MatchesArray;

		// Apply search type filters - using USTRUCT params
		if (Params.searchType == TEXT("name") || Params.searchType == TEXT("all"))
		{
			if (AssetData.AssetName.ToString().Contains(Params.searchTerm))
			{
				bMatches = true;
				
				// Add match detail
				TSharedPtr<FJsonObject> MatchJson = MakeShareable(new FJsonObject);
				MatchJson->SetStringField(TEXT("type"), TEXT("asset_name"));
				MatchJson->SetStringField(TEXT("location"), TEXT("Blueprint Asset"));
				MatchJson->SetStringField(TEXT("context"), FString::Printf(TEXT("Blueprint name '%s' contains '%s'"), 
					*AssetData.AssetName.ToString(), *Params.searchTerm));
				MatchesArray.Add(MakeShareable(new FJsonValueObject(MatchJson)));
			}
		}

		if (Params.searchType == TEXT("parent_class") || Params.searchType == TEXT("all"))
		{
			// Get parent class information from asset tags
			FString ParentClassPath;
			if (AssetData.GetTagValue(TEXT("ParentClass"), ParentClassPath))
			{
				if (ParentClassPath.Contains(Params.searchTerm))
				{
					bMatches = true;
					
					// Add match detail
					TSharedPtr<FJsonObject> MatchJson = MakeShareable(new FJsonObject);
					MatchJson->SetStringField(TEXT("type"), TEXT("parent_class"));
					MatchJson->SetStringField(TEXT("location"), TEXT("Blueprint Asset"));
					MatchJson->SetStringField(TEXT("context"), FString::Printf(TEXT("Parent class '%s' contains '%s'"), 
						*ParentClassPath, *Params.searchTerm));
					MatchesArray.Add(MakeShareable(new FJsonValueObject(MatchJson)));
				}
			}
		}

		// If this Blueprint matches, add it to results
		if (bMatches)
		{
			TotalMatches++;
			
			TSharedPtr<FJsonObject> BlueprintResult = MakeShareable(new FJsonObject);
			BlueprintResult->SetStringField(TEXT("assetPath"), AssetData.GetSoftObjectPath().ToString());
			BlueprintResult->SetStringField(TEXT("assetName"), AssetData.AssetName.ToString());
			BlueprintResult->SetStringField(TEXT("packagePath"), AssetData.PackagePath.ToString());
			
			// Get parent class for display
			FString ParentClassPath;
			AssetData.GetTagValue(TEXT("ParentClass"), ParentClassPath);
			BlueprintResult->SetStringField(TEXT("parentClass"), ParentClassPath);
			
			BlueprintResult->SetArrayField(TEXT("matches"), MatchesArray);
			
			ResultsArray.Add(MakeShareable(new FJsonValueObject(BlueprintResult)));
		}
	}

	// Build final result JSON - output has complex nested structure, so we build JSON manually
	// but we've used USTRUCT for all input processing
	ResultsJson->SetArrayField(TEXT("results"), ResultsArray);
	ResultsJson->SetNumberField(TEXT("totalResults"), TotalMatches);
	
	TSharedPtr<FJsonObject> SearchCriteriaJson = MakeShareable(new FJsonObject);
	SearchCriteriaJson->SetStringField(TEXT("searchType"), Params.searchType);
	SearchCriteriaJson->SetStringField(TEXT("searchTerm"), Params.searchTerm);
	if (!Params.packagePath.IsEmpty())
	{
		SearchCriteriaJson->SetStringField(TEXT("packagePath"), Params.packagePath);
	}
	SearchCriteriaJson->SetBoolField(TEXT("recursive"), Params.bRecursive);
	ResultsJson->SetObjectField(TEXT("searchCriteria"), SearchCriteriaJson);

	// Convert to JSON string
	FString ResultJsonString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResultJsonString);
	FJsonSerializer::Serialize(ResultsJson.ToSharedRef(), Writer);

	Content.text = ResultJsonString;
	
	UE_LOG(LogUnrealMCPServer, Log, TEXT("SearchBlueprints: Completed search, found %d matches"), TotalMatches);
	
	return true;
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

	// Validate: Prevent searches on /Game/ directory without class filters (extremely expensive)
	// Skip this check if package names are specified, as package name searches are more targeted
	bool bHasGamePathOnly = false;
	if (Params.packageNames.Num() == 0 && Params.packagePaths.Num() == 1)
	{
		FString PackagePathStr = Params.packagePaths[0];
		if (PackagePathStr == TEXT("/Game") || PackagePathStr == TEXT("/Game/"))
		{
			bHasGamePathOnly = true;
		}
	}
	
	if (bHasGamePathOnly && ClassPaths.Num() == 0)
	{
		Content.text = TEXT("Error: Searching /Game/ directory without class filters is not allowed as it is extremely expensive. Please provide at least one class filter in the 'classPaths' parameter, or use 'packageNames' to search by specific package names.");
		UE_LOG(LogUnrealMCPServer, Warning, TEXT("SearchAssets: Blocked expensive /Game/ search without class filters"));
		return false;
	}

	UE_LOG(LogUnrealMCPServer, Log, TEXT("SearchAssets: PackagePaths=%d, PackageNames=%d, ClassPaths=%d, Recursive=%s, IncludeTags=%s"), 
		Params.packagePaths.Num(), Params.packageNames.Num(), ClassPaths.Num(), Params.bRecursive ? TEXT("true") : TEXT("false"), Params.bIncludeTags ? TEXT("true") : TEXT("false"));

	// Get Asset Registry
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	// Build filter
	FARFilter Filter;
	
	// Add package paths from USTRUCT
	for (const FString& PackagePathStr : Params.packagePaths)
	{
		Filter.PackagePaths.Add(FName(*PackagePathStr));
	}
	Filter.bRecursivePaths = Params.bRecursive;
	
	// Add package names from USTRUCT
	for (const FString& PackageNameStr : Params.packageNames)
	{
		Filter.PackageNames.Add(FName(*PackageNameStr));
	}
	
	// Add class paths if specified
	if (ClassPaths.Num() > 0)
	{
		Filter.ClassPaths = ClassPaths;
		Filter.bRecursiveClasses = true;
	}

	// Perform asset search
	TArray<FAssetData> AssetDataList;
	AssetRegistry.GetAssets(Filter, AssetDataList);

	UE_LOG(LogUnrealMCPServer, Log, TEXT("SearchAssets: Found %d assets"), AssetDataList.Num());

	// Build results - output has complex nested structure, so we build JSON manually
	// but we've used USTRUCT for all input processing
	TSharedPtr<FJsonObject> ResultsJson = MakeShareable(new FJsonObject);
	TArray<TSharedPtr<FJsonValue>> AssetsArray;

	for (const FAssetData& AssetData : AssetDataList)
	{
		TSharedPtr<FJsonObject> AssetJson = MakeShareable(new FJsonObject);
		AssetDataToJson(AssetData, AssetJson, Params.bIncludeTags);
		AssetsArray.Add(MakeShareable(new FJsonValueObject(AssetJson)));
	}

	ResultsJson->SetArrayField(TEXT("assets"), AssetsArray);
	ResultsJson->SetNumberField(TEXT("count"), AssetsArray.Num());

	// Convert to JSON string
	FString ResultJsonString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResultJsonString);
	FJsonSerializer::Serialize(ResultsJson.ToSharedRef(), Writer);

	Content.text = ResultJsonString;
	
	UE_LOG(LogUnrealMCPServer, Log, TEXT("SearchAssets: Completed search, found %d assets"), AssetsArray.Num());
	
	return true;
}

