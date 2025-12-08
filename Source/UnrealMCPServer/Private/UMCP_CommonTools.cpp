#include "UMCP_CommonTools.h"
#include "UMCP_Server.h"
#include "UMCP_Types.h"
#include "UnrealMCPServerModule.h"
#include "UObject/UnrealType.h"
#include "Exporters/Exporter.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/Blueprint.h"
#include "K2Node.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealType.h"
#include "Editor/UnrealEd/Public/UnrealEd.h"
#include "HAL/PlatformFilemanager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/EngineVersion.h"
#include "Factories/Factory.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Editor/UnrealEd/Public/Editor.h"
#include "Editor/UnrealEd/Public/AutomatedAssetImportData.h"


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
}


void FUMCP_CommonTools::Register(class FUMCP_Server* Server)
{
	{
		FUMCP_ToolDefinition Tool;
		Tool.name = TEXT("search_blueprints");
		Tool.description = TEXT("Search for Blueprint assets based on various criteria including name patterns, parent classes, and package paths.");
		Tool.DoToolCall.BindRaw(this, &FUMCP_CommonTools::SearchBlueprints);
		
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
		Tool.description = TEXT("Export any UObject to a specified format (defaults to T3D).");
		Tool.DoToolCall.BindRaw(this, &FUMCP_CommonTools::ExportAsset);
		
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
		Tool.name = TEXT("export_class_default");
		Tool.description = TEXT("Export the class default object for a given class path. This allows determining default values for a class, since exporting instances of objects do not print values that are identical to the default value.");
		Tool.DoToolCall.BindRaw(this, &FUMCP_CommonTools::ExportClassDefault);
		
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
		Tool.DoToolCall.BindRaw(this, &FUMCP_CommonTools::ImportAsset);
		
		// Generate input schema from USTRUCT with descriptions
		TMap<FString, FString> InputDescriptions;
		InputDescriptions.Add(TEXT("filePath"), TEXT("The path to the file to import (any supported format)"));
		InputDescriptions.Add(TEXT("packagePath"), TEXT("The package path where the object should be created (e.g., '/Game/MyFolder')"));
		TArray<FString> InputRequired;
		InputRequired.Add(TEXT("filePath"));
		InputRequired.Add(TEXT("packagePath"));
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
		Tool.DoToolCall.BindRaw(this, &FUMCP_CommonTools::QueryAsset);
		
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
		Tool.description = TEXT("Search for assets in specified package paths, optionally filtered by class. Returns an array of asset information from the asset registry.");
		Tool.DoToolCall.BindRaw(this, &FUMCP_CommonTools::SearchAssets);
		
		// Generate input schema from USTRUCT with descriptions
		TMap<FString, FString> InputDescriptions;
		InputDescriptions.Add(TEXT("packagePaths"), TEXT("Array of directory/package paths to search for assets (e.g., ['/Game/Blueprints', '/Game/Materials'])"));
		InputDescriptions.Add(TEXT("classPaths"), TEXT("Array of class paths to filter by (e.g., ['/Script/Engine.Blueprint', '/Script/Engine.Texture2D']). If empty, searches all asset types."));
		InputDescriptions.Add(TEXT("bRecursive"), TEXT("Whether to search recursively in subdirectories. Defaults to true."));
		InputDescriptions.Add(TEXT("bIncludeTags"), TEXT("Whether to include asset tags in the response. Defaults to false."));
		TArray<FString> InputRequired;
		InputRequired.Add(TEXT("packagePaths"));
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
	
	{
		FUMCP_ToolDefinition Tool;
		Tool.name = TEXT("get_project_config");
		Tool.description = TEXT("Retrieve project and engine configuration basics including Engine folder, Project folder, Content folder, Log folder, and engine version.");
		Tool.DoToolCall.BindRaw(this, &FUMCP_CommonTools::GetProjectConfig);
		
		// Generate input schema from USTRUCT (empty params struct)
		Tool.inputSchema = UMCP_GenerateJsonSchemaFromStruct(FUMCP_GetProjectConfigParams::StaticStruct());
		
		// Generate output schema from USTRUCT with descriptions
		TMap<FString, FString> OutputDescriptions;
		OutputDescriptions.Add(TEXT("engineVersion"), TEXT("Engine version information"));
		OutputDescriptions.Add(TEXT("paths"), TEXT("Project and engine directory paths"));
		OutputDescriptions.Add(TEXT("full"), TEXT("Full version string"));
		OutputDescriptions.Add(TEXT("major"), TEXT("Major version number"));
		OutputDescriptions.Add(TEXT("minor"), TEXT("Minor version number"));
		OutputDescriptions.Add(TEXT("patch"), TEXT("Patch version number"));
		OutputDescriptions.Add(TEXT("changelist"), TEXT("Changelist number"));
		OutputDescriptions.Add(TEXT("engineDir"), TEXT("Engine directory path"));
		OutputDescriptions.Add(TEXT("projectDir"), TEXT("Project directory path"));
		OutputDescriptions.Add(TEXT("projectContentDir"), TEXT("Project content directory path"));
		OutputDescriptions.Add(TEXT("projectLogDir"), TEXT("Project log directory path"));
		OutputDescriptions.Add(TEXT("projectSavedDir"), TEXT("Project saved directory path"));
		OutputDescriptions.Add(TEXT("projectConfigDir"), TEXT("Project config directory path"));
		OutputDescriptions.Add(TEXT("projectPluginsDir"), TEXT("Project plugins directory path"));
		OutputDescriptions.Add(TEXT("engineContentDir"), TEXT("Engine content directory path"));
		OutputDescriptions.Add(TEXT("enginePluginsDir"), TEXT("Engine plugins directory path"));
		
		TArray<FString> OutputRequired;
		OutputRequired.Add(TEXT("engineVersion"));
		OutputRequired.Add(TEXT("paths"));
		
		TSharedPtr<FJsonObject> ProjectConfigOutputSchema = UMCP_GenerateJsonSchemaFromStruct(FUMCP_GetProjectConfigResult::StaticStruct(), OutputDescriptions, OutputRequired);
		if (ProjectConfigOutputSchema.IsValid())
		{
			Tool.outputSchema = ProjectConfigOutputSchema;
		}
		else
		{
			UE_LOG(LogUnrealMCPServer, Error, TEXT("Failed to generate outputSchema for get_project_config tool"));
		}
		Server->RegisterTool(MoveTemp(Tool));
	}
}

bool FUMCP_CommonTools::SearchBlueprints(TSharedPtr<FJsonObject> arguments, TArray<FUMCP_CallToolResultContent>& OutContent)
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

bool FUMCP_CommonTools::ExportAsset(TSharedPtr<FJsonObject> arguments, TArray<FUMCP_CallToolResultContent>& OutContent)
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

	UObject* Object = LoadObject<UObject>(nullptr, *Params.objectPath);
	if (!Object)
	{
		Result.error = FString::Printf(TEXT("Failed to load Object: %s"), *Params.objectPath);
		if (!UMCP_ToJsonString(Result, Content.text))
		{
			Content.text = TEXT("Failed to serialize error result");
		}
		return false;
	}

	UExporter* Exporter = UExporter::FindExporter(Object, *Params.format);
	if (!Exporter)
	{
		Result.error = FString::Printf(TEXT("Failed to find %s exporter for Object: %s"), *Params.format, *Params.objectPath);
		if (!UMCP_ToJsonString(Result, Content.text))
		{
			Content.text = TEXT("Failed to serialize error result");
		}
		return false;
	}

	FStringOutputDevice OutputDevice;
	const uint32 ExportFlags = PPF_Copy | PPF_ExportsNotFullyQualified;
	UE_LOG(LogUnrealMCPServer, Log, TEXT("Attempting to export Object '%s' to %s format using exporter: %s"), *Params.objectPath, *Params.format, *Exporter->GetClass()->GetName());
	Exporter->ExportText(nullptr, Object, *Params.format, OutputDevice, GWarn, ExportFlags);
	if (OutputDevice.IsEmpty())
	{
		Result.error = FString::Printf(TEXT("ExportText did not produce any output for Object: %s. Using exporter: %s."), *Params.objectPath, *Exporter->GetClass()->GetName());
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
	
	UE_LOG(LogUnrealMCPServer, Log, TEXT("Successfully exported Object '%s' to %s format"), *Params.objectPath, *Params.format);
	return true;
}

bool FUMCP_CommonTools::ExportClassDefault(TSharedPtr<FJsonObject> arguments, TArray<FUMCP_CallToolResultContent>& OutContent)
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

bool FUMCP_CommonTools::ImportAsset(TSharedPtr<FJsonObject> arguments, TArray<FUMCP_CallToolResultContent>& OutContent)
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
	
	if (Params.filePath.IsEmpty())
	{
		Result.error = TEXT("Missing FilePath parameter.");
		
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

	// Convert to absolute path
	FString AbsoluteFilePath = FPaths::ConvertRelativePathToFull(Params.filePath);
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	
	// Verify file exists
	if (!PlatformFile.FileExists(*AbsoluteFilePath))
	{
		Result.error = FString::Printf(TEXT("File not found: %s"), *AbsoluteFilePath);
		Result.filePath = AbsoluteFilePath;
		
		// Convert USTRUCT to JSON string at the end
		if (!UMCP_ToJsonString(Result, Content.text))
		{
			Content.text = TEXT("Failed to serialize result");
		}
		return false;
	}
	
	// Ensure package path starts with /Game or similar
	FString PackagePath = Params.packagePath;
	if (!PackagePath.StartsWith(TEXT("/")))
	{
		PackagePath = TEXT("/Game/") + PackagePath;
	}

	// Get Asset Tools module
	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
	
	// Find a factory that can import this file using FactoryCanImport
	UFactory* ImportFactory = nullptr;
	FString FactoryClassName;
	
	// Try to find a factory that can import this file
	for (UClass* Class : TObjectRange<UClass>())
	{
		if (Class->IsChildOf<UFactory>() && !Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
		{
			UFactory* TestFactory = Class->GetDefaultObject<UFactory>();
			if (TestFactory && TestFactory->FactoryCanImport(AbsoluteFilePath))
			{
				ImportFactory = TestFactory;
				FactoryClassName = Class->GetName();
				UE_LOG(LogUnrealMCPServer, Log, TEXT("Found factory '%s' that can import file: %s"), *FactoryClassName, *AbsoluteFilePath);
				break;
			}
		}
	}
	
	if (!ImportFactory)
	{
		Result.error = FString::Printf(TEXT("Could not find a factory that can import file: %s. The file format may not be supported."), *AbsoluteFilePath);
		Result.filePath = AbsoluteFilePath;
		
		// Convert USTRUCT to JSON string at the end
		if (!UMCP_ToJsonString(Result, Content.text))
		{
			Content.text = TEXT("Failed to serialize result");
		}
		return false;
	}

	// Create automated import data
	UAutomatedAssetImportData* ImportData = NewObject<UAutomatedAssetImportData>();
	ImportData->GroupName = FString::Printf(TEXT("MCP_Import_%s"), *FPaths::GetBaseFilename(AbsoluteFilePath));
	ImportData->Filenames.Add(AbsoluteFilePath);
	ImportData->DestinationPath = PackagePath;
	ImportData->FactoryName = FactoryClassName;
	ImportData->bReplaceExisting = true;
	ImportData->bSkipReadOnly = false;
	
	// Use ImportAssetsAutomated to import the file
	TArray<UObject*> ImportedObjects = AssetToolsModule.Get().ImportAssetsAutomated(ImportData);
	
	if (ImportedObjects.Num() == 0)
	{
		Result.error = TEXT("Import completed but no objects were created. Check the file format and content.");
		Result.filePath = AbsoluteFilePath;
		Result.packagePath = PackagePath;
		Result.factoryClass = FactoryClassName;
		
		// Convert USTRUCT to JSON string at the end
		if (!UMCP_ToJsonString(Result, Content.text))
		{
			Content.text = TEXT("Failed to serialize result");
		}
		return false;
	}
	
	// Build structured result in USTRUCT
	Result.bSuccess = true;
	Result.count = ImportedObjects.Num();
	Result.filePath = AbsoluteFilePath;
	Result.packagePath = PackagePath;
	Result.factoryClass = FactoryClassName;
	
	Result.importedObjects.Empty();
	for (int32 i = 0; i < ImportedObjects.Num(); ++i)
	{
		if (ImportedObjects[i])
		{
			Result.importedObjects.Add(ImportedObjects[i]->GetPathName());
		}
	}
	
	// Convert USTRUCT to JSON string at the end
	if (!UMCP_ToJsonString(Result, Content.text))
	{
		Content.text = TEXT("Failed to serialize result");
		return false;
	}
	UE_LOG(LogUnrealMCPServer, Log, TEXT("Successfully imported %d object(s) from file: %s"), ImportedObjects.Num(), *AbsoluteFilePath);
	
	return true;
}

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
}

bool FUMCP_CommonTools::QueryAsset(TSharedPtr<FJsonObject> arguments, TArray<FUMCP_CallToolResultContent>& OutContent)
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

bool FUMCP_CommonTools::SearchAssets(TSharedPtr<FJsonObject> arguments, TArray<FUMCP_CallToolResultContent>& OutContent)
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
	if (Params.packagePaths.Num() == 0)
	{
		Content.text = TEXT("Missing required parameter: packagePaths (must be a non-empty array)");
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
	bool bHasGamePathOnly = false;
	if (Params.packagePaths.Num() == 1)
	{
		FString PackagePathStr = Params.packagePaths[0];
		if (PackagePathStr == TEXT("/Game") || PackagePathStr == TEXT("/Game/"))
		{
			bHasGamePathOnly = true;
		}
	}
	
	if (bHasGamePathOnly && ClassPaths.Num() == 0)
	{
		Content.text = TEXT("Error: Searching /Game/ directory without class filters is not allowed as it is extremely expensive. Please provide at least one class filter in the 'classPaths' parameter.");
		UE_LOG(LogUnrealMCPServer, Warning, TEXT("SearchAssets: Blocked expensive /Game/ search without class filters"));
		return false;
	}

	UE_LOG(LogUnrealMCPServer, Log, TEXT("SearchAssets: PackagePaths=%d, ClassPaths=%d, Recursive=%s, IncludeTags=%s"), 
		Params.packagePaths.Num(), ClassPaths.Num(), Params.bRecursive ? TEXT("true") : TEXT("false"), Params.bIncludeTags ? TEXT("true") : TEXT("false"));

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

bool FUMCP_CommonTools::GetProjectConfig(TSharedPtr<FJsonObject> arguments, TArray<FUMCP_CallToolResultContent>& OutContent)
{
	auto& Content = OutContent.Add_GetRef(FUMCP_CallToolResultContent());
	Content.type = TEXT("text");

	// Convert JSON to USTRUCT (no params needed, but validate structure)
	FUMCP_GetProjectConfigParams Params;
	if (arguments.IsValid() && !UMCP_CreateFromJsonObject(arguments, Params, true))
	{
		Content.text = TEXT("Invalid parameters");
		return false;
	}

	// Work with USTRUCT throughout
	FUMCP_GetProjectConfigResult Result;
	
	// Get engine version
	FEngineVersion EngineVersion = FEngineVersion::Current();
	Result.engineVersion.full = EngineVersion.ToString();
	Result.engineVersion.major = EngineVersion.GetMajor();
	Result.engineVersion.minor = EngineVersion.GetMinor();
	Result.engineVersion.patch = EngineVersion.GetPatch();
	Result.engineVersion.changelist = EngineVersion.GetChangelist();

	// Get project and engine paths
	Result.paths.engineDir = FPaths::EngineDir();
	Result.paths.projectDir = FPaths::ProjectDir();
	Result.paths.projectContentDir = FPaths::ProjectContentDir();
	Result.paths.projectLogDir = FPaths::ProjectLogDir();
	Result.paths.projectSavedDir = FPaths::ProjectSavedDir();
	Result.paths.projectConfigDir = FPaths::ProjectConfigDir();
	Result.paths.projectPluginsDir = FPaths::ProjectPluginsDir();
	Result.paths.engineContentDir = FPaths::EngineContentDir();
	Result.paths.enginePluginsDir = FPaths::EnginePluginsDir();

	// Convert USTRUCT to JSON string at the end
	if (!UMCP_ToJsonString(Result, Content.text))
	{
		Content.text = TEXT("Failed to serialize result");
		return false;
	}
	
	UE_LOG(LogUnrealMCPServer, Log, TEXT("GetProjectConfig: Retrieved project configuration"));
	
	return true;
}
