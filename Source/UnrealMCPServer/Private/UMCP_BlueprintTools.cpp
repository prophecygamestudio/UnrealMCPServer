#include "UMCP_BlueprintTools.h"
#include "UMCP_Server.h"
#include "UMCP_Types.h"
#include "UnrealMCPServerModule.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/Blueprint.h"
#include "K2Node.h"
#include "UObject/UObjectGlobals.h"
#include "HAL/PlatformFilemanager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Exporters/Exporter.h"
#include "Interfaces/IPluginManager.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

void FUMCP_BlueprintTools::Register(class FUMCP_Server* Server)
{
	{
		FUMCP_ToolDefinition Tool;
		Tool.name = TEXT("search_blueprints");
		Tool.description = TEXT("Search for Blueprint assets based on various criteria including name patterns, parent classes, and package paths. Returns array of Blueprint asset information including paths, names, parent classes, and match details. Use 'name' searchType to find Blueprints by name pattern (e.g., 'BP_Player*'), 'parent_class' to find Blueprints that inherit from a class (e.g., 'Actor', 'Pawn', 'Character'), or 'all' for comprehensive search across all criteria.");
		Tool.DoToolCall.BindRaw(this, &FUMCP_BlueprintTools::SearchBlueprints);
		
		// Generate input schema from USTRUCT with descriptions and enum
		TMap<FString, FString> InputDescriptions;
		InputDescriptions.Add(TEXT("searchType"), TEXT("Type of search to perform. 'name': Find Blueprints by name pattern (e.g., 'BP_Player*' finds all Blueprints starting with 'BP_Player'). 'parent_class': Find Blueprints that inherit from a class (e.g., 'Actor', 'Pawn', 'Character'). 'all': Comprehensive search across all criteria."));
		InputDescriptions.Add(TEXT("searchTerm"), TEXT("Search term to match against. For 'name' type: Blueprint name pattern (e.g., 'BP_Player', 'Enemy'). For 'parent_class' type: Parent class name (e.g., 'Actor', 'Pawn', 'Character'). For 'all' type: Searches both name and parent class."));
		InputDescriptions.Add(TEXT("packagePath"), TEXT("Optional package path to limit search scope. Examples: '/Game/Blueprints' searches in Blueprints folder, '/Game/Characters' searches in Characters folder. Uses Unreal's path format. If not specified, searches entire project."));
		InputDescriptions.Add(TEXT("bRecursive"), TEXT("Whether to search recursively in subfolders. Defaults to true. Set to false to search only the specified packagePath directory without subdirectories."));
		InputDescriptions.Add(TEXT("maxResults"), TEXT("Maximum number of results to return. Defaults to 0 (no limit). Use with offset for paging through large result sets. Recommended for large searches to limit response size."));
		InputDescriptions.Add(TEXT("offset"), TEXT("Number of results to skip before returning results. Defaults to 0. Use with maxResults for paging: first page uses offset=0, second page uses offset=maxResults, etc."));
		TArray<FString> InputRequired;
		InputRequired.Add(TEXT("searchType"));
		InputRequired.Add(TEXT("searchTerm"));
		TMap<FString, TArray<FString>> EnumValues;
		EnumValues.Add(TEXT("searchType"), { TEXT("name"), TEXT("parent_class"), TEXT("all") });
		Tool.inputSchema = UMCP_GenerateJsonSchemaFromStruct<FUMCP_SearchBlueprintsParams>(InputDescriptions, InputRequired, EnumValues);
		
		// Output schema is complex with nested objects, so we'll keep it as manual JSON for now
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
			TEXT("\"totalResults\":{\"type\":\"number\",\"description\":\"Number of results in this page\"},")
			TEXT("\"totalCount\":{\"type\":\"number\",\"description\":\"Total number of matching results\"},")
			TEXT("\"offset\":{\"type\":\"number\",\"description\":\"Offset used for this page\"},")
			TEXT("\"hasMore\":{\"type\":\"boolean\",\"description\":\"Whether there are more results available\"},")
			TEXT("\"searchCriteria\":{\"type\":\"object\",\"description\":\"The search criteria used\",\"properties\":{")
			TEXT("\"searchType\":{\"type\":\"string\"},")
			TEXT("\"searchTerm\":{\"type\":\"string\"},")
			TEXT("\"packagePath\":{\"type\":\"string\",\"description\":\"Optional package path if specified\"},")
			TEXT("\"recursive\":{\"type\":\"boolean\"}")
			TEXT("},\"required\":[\"searchType\",\"searchTerm\",\"recursive\"]")
			TEXT("}},")
			TEXT("\"required\":[\"results\",\"totalResults\",\"totalCount\",\"offset\",\"hasMore\",\"searchCriteria\"]")
			TEXT("}");
		TSharedPtr<FJsonObject> ParsedSchema = UMCP_FromJsonStr(SearchOutputSchema);
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
		Tool.name = TEXT("export_blueprint_markdown");
		
		// Check if BP2AI plugin is available for markdown export support
		TSharedPtr<IPlugin> BP2AIPlugin = IPluginManager::Get().FindPlugin(TEXT("BP2AI"));
		bool bBP2AIAvailable = BP2AIPlugin.IsValid() && BP2AIPlugin->IsEnabled();
		
		// Build description based on plugin availability
		FString Description = TEXT("Export Blueprint asset(s) to markdown format for graph inspection. This is the recommended method for inspecting Blueprint graph structure, as Blueprint exports are too large to return directly in responses. The markdown export provides complete Blueprint graph information including nodes, variables, functions, and events. Files are saved to disk at the specified output folder path. Each Blueprint is exported to a separate markdown file named after the asset. Returns array of successfully exported file paths. ");
		if (!bBP2AIAvailable)
		{
			Description += TEXT("WARNING: BP2AI plugin is not available. Markdown export may not be supported. ");
		}
		Description += TEXT("After export, agents should read the markdown file using standard file system tools, then parse and optionally flatten the markdown to understand the graph structure. The MCP cannot perform the simplification/flattening step - this must be done by the agent.");
		Tool.description = Description;
		Tool.DoToolCall.BindRaw(this, &FUMCP_BlueprintTools::ExportBlueprintMarkdown);
		
		// Generate input schema from USTRUCT with descriptions
		TMap<FString, FString> InputDescriptions;
		InputDescriptions.Add(TEXT("blueprintPaths"), TEXT("Array of Blueprint object paths to export. Each path should be in format '/Game/Folder/BlueprintName' or '/Game/Folder/BlueprintName.BlueprintName'. Examples: ['/Game/Blueprints/BP_Player.BP_Player', '/Game/Characters/BP_Enemy.BP_Enemy']. All paths must be valid Blueprint assets."));
		InputDescriptions.Add(TEXT("outputFolder"), TEXT("The absolute or relative folder path where exported markdown files should be saved. Examples: 'C:/Exports/Blueprints', './Exports', '/tmp/exports'. The folder will be created if it doesn't exist. Each Blueprint is exported to a separate markdown file named after the asset (e.g., 'BP_Player.md', 'BP_Enemy.md')."));
		TArray<FString> InputRequired;
		InputRequired.Add(TEXT("blueprintPaths"));
		InputRequired.Add(TEXT("outputFolder"));
		Tool.inputSchema = UMCP_GenerateJsonSchemaFromStruct<FUMCP_ExportBlueprintMarkdownParams>(InputDescriptions, InputRequired);
		
		// Generate output schema from USTRUCT with descriptions
		TMap<FString, FString> OutputDescriptions;
		OutputDescriptions.Add(TEXT("bSuccess"), TEXT("Whether the export operation was successful overall"));
		OutputDescriptions.Add(TEXT("exportedCount"), TEXT("Number of Blueprints successfully exported"));
		OutputDescriptions.Add(TEXT("failedCount"), TEXT("Number of Blueprints that failed to export"));
		OutputDescriptions.Add(TEXT("exportedPaths"), TEXT("Array of file paths for successfully exported markdown files"));
		OutputDescriptions.Add(TEXT("failedPaths"), TEXT("Array of Blueprint paths that failed to export"));
		OutputDescriptions.Add(TEXT("error"), TEXT("Error message if bSuccess is false"));
		TArray<FString> OutputRequired;
		OutputRequired.Add(TEXT("bSuccess"));
		OutputRequired.Add(TEXT("exportedCount"));
		OutputRequired.Add(TEXT("failedCount"));
		TSharedPtr<FJsonObject> ExportOutputSchema = UMCP_GenerateJsonSchemaFromStruct<FUMCP_ExportBlueprintMarkdownResult>(OutputDescriptions, OutputRequired);
		if (ExportOutputSchema.IsValid())
		{
			Tool.outputSchema = ExportOutputSchema;
		}
		else
		{
			UE_LOG(LogUnrealMCPServer, Error, TEXT("Failed to generate outputSchema for export_blueprint_markdown tool"));
		}
		Server->RegisterTool(MoveTemp(Tool));
	}
}

bool FUMCP_BlueprintTools::SearchBlueprints(TSharedPtr<FJsonObject> arguments, TArray<FUMCP_CallToolResultContent>& OutContent)
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

	// Build all matching results first (before paging)
	TArray<TSharedPtr<FJsonValue>> AllResultsArray;
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
			
			AllResultsArray.Add(MakeShareable(new FJsonValueObject(BlueprintResult)));
		}
	}

	// Apply paging: calculate total count before limiting
	int32 TotalCount = TotalMatches;
	int32 StartIndex = FMath::Max(0, Params.offset);
	int32 EndIndex = AllResultsArray.Num();
	
	// Apply maxResults limit if specified
	if (Params.maxResults > 0)
	{
		EndIndex = FMath::Min(StartIndex + Params.maxResults, AllResultsArray.Num());
	}
	
	// Extract the page of results
	TArray<TSharedPtr<FJsonValue>> PagedResultsArray;
	if (StartIndex < AllResultsArray.Num())
	{
		for (int32 i = StartIndex; i < EndIndex; ++i)
		{
			PagedResultsArray.Add(AllResultsArray[i]);
		}
	}
	
	UE_LOG(LogUnrealMCPServer, Log, TEXT("SearchBlueprints: Returning %d results (offset=%d, maxResults=%d, total=%d)"), 
		PagedResultsArray.Num(), Params.offset, Params.maxResults, TotalCount);

	// Build final result JSON - output has complex nested structure, so we build JSON manually
	// but we've used USTRUCT for all input processing
	TSharedPtr<FJsonObject> ResultsJson = MakeShareable(new FJsonObject);
	ResultsJson->SetArrayField(TEXT("results"), PagedResultsArray);
	ResultsJson->SetNumberField(TEXT("totalResults"), PagedResultsArray.Num());
	ResultsJson->SetNumberField(TEXT("totalCount"), TotalCount);
	ResultsJson->SetNumberField(TEXT("offset"), Params.offset);
	ResultsJson->SetBoolField(TEXT("hasMore"), EndIndex < TotalCount);
	
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
	
	UE_LOG(LogUnrealMCPServer, Log, TEXT("SearchBlueprints: Completed search, found %d matches (returning %d)"), TotalCount, PagedResultsArray.Num());
	
	return true;
}

bool FUMCP_BlueprintTools::ExportBlueprintMarkdown(TSharedPtr<FJsonObject> arguments, TArray<FUMCP_CallToolResultContent>& OutContent)
{
	auto& Content = OutContent.Add_GetRef(FUMCP_CallToolResultContent());
	Content.type = TEXT("text");

	// Convert JSON to USTRUCT at the start
	FUMCP_ExportBlueprintMarkdownParams Params;
	if (!arguments.IsValid() || !UMCP_CreateFromJsonObject(arguments, Params))
	{
		FUMCP_ExportBlueprintMarkdownResult ErrorResult;
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
	FUMCP_ExportBlueprintMarkdownResult Result;
	Result.bSuccess = false;
	Result.exportedCount = 0;
	Result.failedCount = 0;

	if (Params.blueprintPaths.Num() == 0)
	{
		Result.error = TEXT("Missing or empty blueprintPaths parameter.");
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

	UE_LOG(LogUnrealMCPServer, Log, TEXT("ExportBlueprintMarkdown: Exporting %d Blueprints to folder: %s"), 
		Params.blueprintPaths.Num(), *AbsoluteOutputFolder);

	// Process each Blueprint
	for (const FString& BlueprintPath : Params.blueprintPaths)
	{
		if (BlueprintPath.IsEmpty())
		{
			Result.failedCount++;
			Result.failedPaths.Add(TEXT(""));
			UE_LOG(LogUnrealMCPServer, Warning, TEXT("ExportBlueprintMarkdown: Skipping empty Blueprint path"));
			continue;
		}

		// Verify this is a Blueprint
		UObject* Object = LoadObject<UObject>(nullptr, *BlueprintPath);
		if (!Object)
		{
			Result.failedCount++;
			Result.failedPaths.Add(BlueprintPath);
			UE_LOG(LogUnrealMCPServer, Warning, TEXT("ExportBlueprintMarkdown: Failed to load Blueprint: %s"), *BlueprintPath);
			continue;
		}

		if (!Object->IsA<UBlueprint>())
		{
			Result.failedCount++;
			Result.failedPaths.Add(BlueprintPath);
			UE_LOG(LogUnrealMCPServer, Warning, TEXT("ExportBlueprintMarkdown: Object is not a Blueprint: %s"), *BlueprintPath);
			continue;
		}

		// Use helper function to export Blueprint to markdown
		FString ExportedMarkdown;
		FString ExportError;
		if (!ExportBlueprintToMarkdown(BlueprintPath, ExportedMarkdown, ExportError))
		{
			Result.failedCount++;
			Result.failedPaths.Add(BlueprintPath);
			UE_LOG(LogUnrealMCPServer, Warning, TEXT("ExportBlueprintMarkdown: Failed to export Blueprint '%s': %s"), *BlueprintPath, *ExportError);
			continue;
		}

		// Generate filename from Blueprint path
		// Extract object name from path (e.g., "/Game/MyBlueprint.MyBlueprint" -> "MyBlueprint")
		FString BlueprintName;
		int32 LastDotIndex;
		if (BlueprintPath.FindLastChar(TEXT('.'), LastDotIndex))
		{
			FString PathBeforeDot = BlueprintPath.Left(LastDotIndex);
			int32 LastSlashIndex;
			if (PathBeforeDot.FindLastChar(TEXT('/'), LastSlashIndex))
			{
				BlueprintName = PathBeforeDot.Mid(LastSlashIndex + 1);
			}
			else
			{
				BlueprintName = PathBeforeDot;
			}
		}
		else
		{
			// Fallback: use last part of path
			int32 LastSlashIndex;
			if (BlueprintPath.FindLastChar(TEXT('/'), LastSlashIndex))
			{
				BlueprintName = BlueprintPath.Mid(LastSlashIndex + 1);
			}
			else
			{
				BlueprintName = BlueprintPath;
			}
		}

		// Sanitize filename (remove invalid characters)
		BlueprintName = BlueprintName.Replace(TEXT(" "), TEXT("_"));
		BlueprintName = BlueprintName.Replace(TEXT("."), TEXT("_"));
		
		// Build full file path
		FString FileName = BlueprintName + TEXT(".md");
		FString FullFilePath = FPaths::Combine(AbsoluteOutputFolder, FileName);

		// Handle filename collisions by appending a number
		FString FinalFilePath = FullFilePath;
		int32 Counter = 1;
		while (PlatformFile.FileExists(*FinalFilePath))
		{
			FinalFilePath = FPaths::Combine(AbsoluteOutputFolder, FString::Printf(TEXT("%s_%d.md"), *BlueprintName, Counter));
			Counter++;
		}

		// Write file
		if (!FFileHelper::SaveStringToFile(ExportedMarkdown, *FinalFilePath))
		{
			Result.failedCount++;
			Result.failedPaths.Add(BlueprintPath);
			UE_LOG(LogUnrealMCPServer, Warning, TEXT("ExportBlueprintMarkdown: Failed to write file: %s for Blueprint: %s"), *FinalFilePath, *BlueprintPath);
			continue;
		}

		// Success
		Result.exportedCount++;
		Result.exportedPaths.Add(FinalFilePath);
		UE_LOG(LogUnrealMCPServer, Log, TEXT("ExportBlueprintMarkdown: Successfully exported Blueprint '%s' to file: %s"), *BlueprintPath, *FinalFilePath);
	}

	// Overall success if at least one Blueprint was exported
	Result.bSuccess = (Result.exportedCount > 0);
	if (!Result.bSuccess && Result.failedCount > 0)
	{
		Result.error = FString::Printf(TEXT("All %d Blueprints failed to export"), Result.failedCount);
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

	UE_LOG(LogUnrealMCPServer, Log, TEXT("ExportBlueprintMarkdown: Completed export. Exported: %d, Failed: %d"), 
		Result.exportedCount, Result.failedCount);
	
	return true;
}

bool FUMCP_BlueprintTools::ExportBlueprintToMarkdown(const FString& ObjectPath, FString& OutExportedText, FString& OutError)
{
	OutExportedText.Empty();
	OutError.Empty();

	if (ObjectPath.IsEmpty())
	{
		OutError = TEXT("ObjectPath is empty");
		return false;
	}

	// Load the Blueprint object
	UObject* Object = LoadObject<UObject>(nullptr, *ObjectPath);
	if (!Object)
	{
		OutError = FString::Printf(TEXT("Failed to load Blueprint: %s"), *ObjectPath);
		return false;
	}

	// Verify it's a Blueprint
	if (!Object->IsA<UBlueprint>())
	{
		OutError = FString::Printf(TEXT("Object is not a Blueprint: %s"), *ObjectPath);
		return false;
	}

	// Find markdown exporter
	UExporter* Exporter = UExporter::FindExporter(Object, TEXT("md"));
	if (!Exporter)
	{
		OutError = FString::Printf(TEXT("Failed to find markdown exporter for Blueprint: %s. BP2AI plugin may not be available."), *ObjectPath);
		return false;
	}

	// Export to markdown
	FStringOutputDevice OutputDevice;
	const uint32 ExportFlags = PPF_Copy | PPF_ExportsNotFullyQualified;
	UE_LOG(LogUnrealMCPServer, Verbose, TEXT("ExportBlueprintToMarkdown: Exporting Blueprint '%s' to markdown format using exporter: %s"), 
		*ObjectPath, *Exporter->GetClass()->GetName());
	Exporter->ExportText(nullptr, Object, TEXT("md"), OutputDevice, GWarn, ExportFlags);
	
	if (OutputDevice.IsEmpty())
	{
		OutError = FString::Printf(TEXT("ExportText did not produce any output for Blueprint: %s. Using exporter: %s."), 
			*ObjectPath, *Exporter->GetClass()->GetName());
		return false;
	}

	OutExportedText = OutputDevice;
	return true;
}

