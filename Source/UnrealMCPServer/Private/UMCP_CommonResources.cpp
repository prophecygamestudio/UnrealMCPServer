#include "UMCP_CommonResources.h"
#include "UMCP_Server.h"
#include "UMCP_Types.h"
#include "UMCP_UriTemplate.h" // For FUMCP_UriTemplate
#include "UnrealMCPServerModule.h"
#include "Engine/Blueprint.h" // Required for UBlueprint
#include "Exporters/Exporter.h" // Required for UExporter
#include "UObject/UObjectGlobals.h" // Required for LoadObject
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Interfaces/IPluginManager.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

FString FUMCP_CommonResources::GetResourcesPath()
{
	// Get plugin directory
	TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("UnrealMCPServer"));
	if (!Plugin.IsValid())
	{
		UE_LOG(LogUnrealMCPServer, Error, TEXT("Failed to find UnrealMCPServer plugin"));
		return FString();
	}
	
	FString PluginBaseDir = Plugin->GetBaseDir();
	FString ResourcesPath = FPaths::Combine(PluginBaseDir, TEXT("Resources"));
	return ResourcesPath;
}

bool FUMCP_CommonResources::LoadResourcesFromJson(class FUMCP_Server* Server)
{
	FString ResourcesPath = GetResourcesPath();
	if (ResourcesPath.IsEmpty())
	{
		UE_LOG(LogUnrealMCPServer, Error, TEXT("Failed to get Resources path"));
		return false;
	}
	
	FString ResourcesJsonPath = FPaths::Combine(ResourcesPath, TEXT("resources.json"));
	
	FString JsonContent;
	if (!FFileHelper::LoadFileToString(JsonContent, *ResourcesJsonPath))
	{
		UE_LOG(LogUnrealMCPServer, Error, TEXT("Failed to load resources.json from: %s"), *ResourcesJsonPath);
		return false;
	}
	
	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonContent);
	
	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		UE_LOG(LogUnrealMCPServer, Error, TEXT("Failed to parse resources.json"));
		return false;
	}
	
	// Load resource templates
	const TArray<TSharedPtr<FJsonValue>>* TemplatesArray;
	if (JsonObject->TryGetArrayField(TEXT("resourceTemplates"), TemplatesArray))
	{
		int32 RegisteredCount = 0;
		for (const TSharedPtr<FJsonValue>& TemplateValue : *TemplatesArray)
		{
			const TSharedPtr<FJsonObject>* TemplateObject;
			if (!TemplateValue->TryGetObject(TemplateObject) || !TemplateObject->IsValid())
			{
				continue;
			}
			
			FUMCP_ResourceTemplateDefinition TemplateDef;
			
			// Get required fields
			if (!(*TemplateObject)->TryGetStringField(TEXT("name"), TemplateDef.name))
			{
				UE_LOG(LogUnrealMCPServer, Warning, TEXT("Resource template missing 'name' field, skipping"));
				continue;
			}
			
			if (!(*TemplateObject)->TryGetStringField(TEXT("uriTemplate"), TemplateDef.uriTemplate))
			{
				UE_LOG(LogUnrealMCPServer, Warning, TEXT("Resource template missing 'uriTemplate' field, skipping"));
				continue;
			}
			
			// Get optional fields
			(*TemplateObject)->TryGetStringField(TEXT("description"), TemplateDef.description);
			(*TemplateObject)->TryGetStringField(TEXT("mimeType"), TemplateDef.mimeType);
			
			// Bind handler based on URI template
			if (TemplateDef.uriTemplate == TEXT("unreal+t3d://{filepath}"))
			{
				TemplateDef.ReadResource.BindRaw(this, &FUMCP_CommonResources::HandleT3DResourceRequest);
			}
			else if (TemplateDef.uriTemplate == TEXT("unreal+md://{filepath}"))
			{
				TemplateDef.ReadResource.BindRaw(this, &FUMCP_CommonResources::HandleMarkdownResourceRequest);
			}
			else
			{
				UE_LOG(LogUnrealMCPServer, Warning, TEXT("Unknown resource template URI: %s, no handler bound"), *TemplateDef.uriTemplate);
				continue;
			}
			
			if (Server->RegisterResourceTemplate(MoveTemp(TemplateDef)))
			{
				RegisteredCount++;
				UE_LOG(LogUnrealMCPServer, Log, TEXT("Registered resource template: %s"), *TemplateDef.name);
			}
			else
			{
				UE_LOG(LogUnrealMCPServer, Error, TEXT("Failed to register resource template: %s"), *TemplateDef.name);
			}
		}
		
		UE_LOG(LogUnrealMCPServer, Log, TEXT("Loaded %d resource templates from resources.json"), RegisteredCount);
		return RegisteredCount > 0;
	}
	
	return false;
}

void FUMCP_CommonResources::Register(class FUMCP_Server* Server)
{
	UE_LOG(LogUnrealMCPServer, Log, TEXT("Registering common MCP resources."));

	// Try to load from JSON first
	if (LoadResourcesFromJson(Server))
	{
		UE_LOG(LogUnrealMCPServer, Log, TEXT("Successfully loaded resources from JSON"));
		return;
	}
	
	// Fallback to hardcoded definitions if JSON loading fails
	UE_LOG(LogUnrealMCPServer, Warning, TEXT("Failed to load resources from JSON, using hardcoded fallback"));
	
	// Register the resource template for discovery AND functionality
	{
		FUMCP_ResourceTemplateDefinition T3DTemplateDefinition;
		T3DTemplateDefinition.name = TEXT("Blueprint T3D Exporter");
		T3DTemplateDefinition.description = TEXT("Exports the T3D representation of an Unreal Engine Blueprint asset specified by its path using the unreal+t3d://{filepath} URI scheme.");
		T3DTemplateDefinition.mimeType = TEXT("application/vnd.unreal.t3d");
		T3DTemplateDefinition.uriTemplate = TEXT("unreal+t3d://{filepath}");
		// Bind the actual handler for this templated resource
		T3DTemplateDefinition.ReadResource.BindRaw(this, &FUMCP_CommonResources::HandleT3DResourceRequest);

		if (Server->RegisterResourceTemplate(MoveTemp(T3DTemplateDefinition)))
		{
			UE_LOG(LogUnrealMCPServer, Log, TEXT("Registered T3D Blueprint Resource Template (unreal+t3d://{filepath}) for discovery and handling."));
		}
		else
		{
			UE_LOG(LogUnrealMCPServer, Error, TEXT("Failed to register T3D Blueprint Resource Template."));
		}
	}

	// Register the markdown resource template for Blueprint summaries
	{
		FUMCP_ResourceTemplateDefinition MarkdownTemplateDefinition;
		MarkdownTemplateDefinition.name = TEXT("Blueprint Markdown Summary");
		MarkdownTemplateDefinition.description = TEXT("Exports the markdown representation of an Unreal Engine Blueprint asset specified by its path using the unreal+md://{filepath} URI scheme. Provides a structured summary of the Blueprint's graph, variables, functions, and events.");
		MarkdownTemplateDefinition.mimeType = TEXT("text/markdown");
		MarkdownTemplateDefinition.uriTemplate = TEXT("unreal+md://{filepath}");
		// Bind the actual handler for this templated resource
		MarkdownTemplateDefinition.ReadResource.BindRaw(this, &FUMCP_CommonResources::HandleMarkdownResourceRequest);

		if (Server->RegisterResourceTemplate(MoveTemp(MarkdownTemplateDefinition)))
		{
			UE_LOG(LogUnrealMCPServer, Log, TEXT("Registered Markdown Blueprint Resource Template (unreal+md://{filepath}) for discovery and handling."));
		}
		else
		{
			UE_LOG(LogUnrealMCPServer, Error, TEXT("Failed to register Markdown Blueprint Resource Template."));
		}
	}
}

bool FUMCP_CommonResources::HandleT3DResourceRequest(const FUMCP_UriTemplate& UriTemplate, const FUMCP_UriTemplateMatch& Match, TArray<FUMCP_ReadResourceResultContent>& OutContent)
{
	auto& Content = OutContent.AddDefaulted_GetRef();
	Content.uri = Match.Uri;
	
	const TArray<FString>* FilePathPtr = Match.Variables.Find(TEXT("filepath"));
	if (!FilePathPtr || FilePathPtr->IsEmpty() || (*FilePathPtr)[0].IsEmpty())
	{
		UE_LOG(LogUnrealMCPServer, Warning, TEXT("HandleT3DResourceRequest: 'filepath' not found in URI '%s' after matching template '%s'."), *Match.Uri, *UriTemplate.GetUriTemplateStr());
        Content.mimeType = TEXT("text/plain");
        Content.text = TEXT("Error: Missing 'filepath' parameter in URI.");
		return false; 
	}
	
	const FString& BlueprintPath = (*FilePathPtr)[0];
	UE_LOG(LogUnrealMCPServer, Log, TEXT("HandleT3DResourceRequest: Attempting to export Blueprint '%s' from URI '%s'."), *BlueprintPath, *Match.Uri);

	UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
	if (!Blueprint)
	{
		UE_LOG(LogUnrealMCPServer, Warning, TEXT("Failed to load Blueprint: %s"), *BlueprintPath);
        Content.mimeType = TEXT("text/plain");
        Content.text = FString::Printf(TEXT("Error: Failed to load Blueprint: %s"), *BlueprintPath);
		return false;
	}

	UExporter* Exporter = UExporter::FindExporter(Blueprint, TEXT("T3D"));
	if (!Exporter)
	{
		UE_LOG(LogUnrealMCPServer, Warning, TEXT("Failed to find T3D exporter for Blueprint: %s"), *BlueprintPath);
        Content.mimeType = TEXT("text/plain");
        Content.text = FString::Printf(TEXT("Error: Failed to find T3D exporter for Blueprint: %s"), *BlueprintPath);
		return false;
	}

	FStringOutputDevice OutputDevice;
	const uint32 ExportFlags = PPF_Copy | PPF_ExportsNotFullyQualified;
	Exporter->ExportText(nullptr, Blueprint, TEXT("T3D"), OutputDevice, GWarn, ExportFlags);

	if (OutputDevice.IsEmpty())
	{
		UE_LOG(LogUnrealMCPServer, Warning, TEXT("ExportText did not produce any output for Blueprint: %s. Using exporter: %s."), *BlueprintPath, *Exporter->GetClass()->GetName());
        Content.mimeType = TEXT("text/plain");
        Content.text = FString::Printf(TEXT("Error: ExportText did not produce any output for Blueprint: %s."), *BlueprintPath);
		return false;
	}

	Content.mimeType = TEXT("application/vnd.unreal.t3d");
	Content.text = OutputDevice;
	
	UE_LOG(LogUnrealMCPServer, Log, TEXT("Successfully exported Blueprint '%s' to T3D via URI '%s'. Output size: %d"), *BlueprintPath, *Match.Uri, Content.text.Len());
	return true;
}

bool FUMCP_CommonResources::HandleMarkdownResourceRequest(const FUMCP_UriTemplate& UriTemplate, const FUMCP_UriTemplateMatch& Match, TArray<FUMCP_ReadResourceResultContent>& OutContent)
{
	auto& Content = OutContent.AddDefaulted_GetRef();
	Content.uri = Match.Uri;
	
	const TArray<FString>* FilePathPtr = Match.Variables.Find(TEXT("filepath"));
	if (!FilePathPtr || FilePathPtr->IsEmpty() || (*FilePathPtr)[0].IsEmpty())
	{
		UE_LOG(LogUnrealMCPServer, Warning, TEXT("HandleMarkdownResourceRequest: 'filepath' not found in URI '%s' after matching template '%s'."), *Match.Uri, *UriTemplate.GetUriTemplateStr());
        Content.mimeType = TEXT("text/plain");
        Content.text = TEXT("Error: Missing 'filepath' parameter in URI.");
		return false; 
	}
	
	const FString& BlueprintPath = (*FilePathPtr)[0];
	UE_LOG(LogUnrealMCPServer, Log, TEXT("HandleMarkdownResourceRequest: Attempting to export Blueprint '%s' to markdown from URI '%s'."), *BlueprintPath, *Match.Uri);

	// Load the Blueprint object
	UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
	if (!Blueprint)
	{
		UE_LOG(LogUnrealMCPServer, Warning, TEXT("Failed to load Blueprint: %s"), *BlueprintPath);
        Content.mimeType = TEXT("text/plain");
        Content.text = FString::Printf(TEXT("Error: Failed to load Blueprint: %s"), *BlueprintPath);
		return false;
	}

	// Find markdown exporter
	UExporter* Exporter = UExporter::FindExporter(Blueprint, TEXT("md"));
	if (!Exporter)
	{
		UE_LOG(LogUnrealMCPServer, Warning, TEXT("Failed to find markdown exporter for Blueprint: %s. BP2AI plugin may not be available."), *BlueprintPath);
        Content.mimeType = TEXT("text/plain");
        Content.text = FString::Printf(TEXT("Error: Failed to find markdown exporter for Blueprint: %s. BP2AI plugin may not be available."), *BlueprintPath);
		return false;
	}

	// Export to markdown
	FStringOutputDevice OutputDevice;
	const uint32 ExportFlags = PPF_Copy | PPF_ExportsNotFullyQualified;
	UE_LOG(LogUnrealMCPServer, Verbose, TEXT("HandleMarkdownResourceRequest: Exporting Blueprint '%s' to markdown format using exporter: %s"), 
		*BlueprintPath, *Exporter->GetClass()->GetName());
	Exporter->ExportText(nullptr, Blueprint, TEXT("md"), OutputDevice, GWarn, ExportFlags);
	
	if (OutputDevice.IsEmpty())
	{
		UE_LOG(LogUnrealMCPServer, Warning, TEXT("ExportText did not produce any output for Blueprint: %s. Using exporter: %s."), 
			*BlueprintPath, *Exporter->GetClass()->GetName());
        Content.mimeType = TEXT("text/plain");
        Content.text = FString::Printf(TEXT("Error: ExportText did not produce any output for Blueprint: %s."), *BlueprintPath);
		return false;
	}

	Content.mimeType = TEXT("text/markdown");
	Content.text = OutputDevice;
	
	UE_LOG(LogUnrealMCPServer, Log, TEXT("Successfully exported Blueprint '%s' to markdown via URI '%s'. Output size: %d"), *BlueprintPath, *Match.Uri, Content.text.Len());
	return true;
}
