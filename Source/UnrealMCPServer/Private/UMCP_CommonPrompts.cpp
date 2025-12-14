#include "UMCP_CommonPrompts.h"
#include "UMCP_Server.h"
#include "UMCP_Types.h"
#include "UnrealMCPServerModule.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Interfaces/IPluginManager.h"

FString FUMCP_CommonPrompts::GetResourcesPath()
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

bool FUMCP_CommonPrompts::LoadPromptsFromJson(class FUMCP_Server* Server)
{
	FString ResourcesPath = GetResourcesPath();
	if (ResourcesPath.IsEmpty())
	{
		UE_LOG(LogUnrealMCPServer, Error, TEXT("Failed to get Resources path"));
		return false;
	}
	
	FString PromptsJsonPath = FPaths::Combine(ResourcesPath, TEXT("prompts.json"));
	
	FString JsonContent;
	if (!FFileHelper::LoadFileToString(JsonContent, *PromptsJsonPath))
	{
		UE_LOG(LogUnrealMCPServer, Error, TEXT("Failed to load prompts.json from: %s"), *PromptsJsonPath);
		return false;
	}
	
	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonContent);
	
	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		UE_LOG(LogUnrealMCPServer, Error, TEXT("Failed to parse prompts.json"));
		return false;
	}
	
	const TArray<TSharedPtr<FJsonValue>>* PromptsArray;
	if (!JsonObject->TryGetArrayField(TEXT("prompts"), PromptsArray))
	{
		UE_LOG(LogUnrealMCPServer, Error, TEXT("prompts.json missing 'prompts' array"));
		return false;
	}
	
	int32 RegisteredCount = 0;
	for (const TSharedPtr<FJsonValue>& PromptValue : *PromptsArray)
	{
		const TSharedPtr<FJsonObject>* PromptObject;
		if (!PromptValue->TryGetObject(PromptObject) || !PromptObject->IsValid())
		{
			continue;
		}
		
		FUMCP_PromptDefinitionInternal Prompt;
		
		// Get name
		if (!(*PromptObject)->TryGetStringField(TEXT("name"), Prompt.name))
		{
			UE_LOG(LogUnrealMCPServer, Warning, TEXT("Prompt missing 'name' field, skipping"));
			continue;
		}
		
		// Get title
		(*PromptObject)->TryGetStringField(TEXT("title"), Prompt.title);
		
		// Get description
		(*PromptObject)->TryGetStringField(TEXT("description"), Prompt.description);
		
		// Get arguments
		const TArray<TSharedPtr<FJsonValue>>* ArgumentsArray;
		if ((*PromptObject)->TryGetArrayField(TEXT("arguments"), ArgumentsArray))
		{
			for (const TSharedPtr<FJsonValue>& ArgValue : *ArgumentsArray)
			{
				const TSharedPtr<FJsonObject>* ArgObject;
				if (ArgValue->TryGetObject(ArgObject) && ArgObject->IsValid())
				{
					FUMCP_PromptArgument Arg;
					(*ArgObject)->TryGetStringField(TEXT("name"), Arg.name);
					(*ArgObject)->TryGetStringField(TEXT("description"), Arg.description);
					(*ArgObject)->TryGetBoolField(TEXT("required"), Arg.required);
					Prompt.arguments.Add(Arg);
				}
			}
		}
		
		// Get template (stored for use in handlers)
		FString Template;
		if ((*PromptObject)->TryGetStringField(TEXT("template"), Template))
		{
			PromptTemplates.Add(Prompt.name, Template);
		}
		
		// Bind handler based on prompt name
		if (Prompt.name == TEXT("analyze_blueprint"))
		{
			Prompt.GetPrompt.BindRaw(this, &FUMCP_CommonPrompts::HandleAnalyzeBlueprintPrompt);
		}
		else if (Prompt.name == TEXT("refactor_blueprint"))
		{
			Prompt.GetPrompt.BindRaw(this, &FUMCP_CommonPrompts::HandleRefactorBlueprintPrompt);
		}
		else if (Prompt.name == TEXT("audit_assets"))
		{
			Prompt.GetPrompt.BindRaw(this, &FUMCP_CommonPrompts::HandleAuditAssetsPrompt);
		}
		else if (Prompt.name == TEXT("create_blueprint"))
		{
			Prompt.GetPrompt.BindRaw(this, &FUMCP_CommonPrompts::HandleCreateBlueprintPrompt);
		}
		else if (Prompt.name == TEXT("analyze_performance"))
		{
			Prompt.GetPrompt.BindRaw(this, &FUMCP_CommonPrompts::HandleAnalyzePerformancePrompt);
		}
		else
		{
			UE_LOG(LogUnrealMCPServer, Warning, TEXT("Unknown prompt name: %s, no handler bound"), *Prompt.name);
			continue;
		}
		
		if (Server->RegisterPrompt(MoveTemp(Prompt)))
		{
			RegisteredCount++;
			UE_LOG(LogUnrealMCPServer, Log, TEXT("Registered prompt: %s"), *Prompt.name);
		}
		else
		{
			UE_LOG(LogUnrealMCPServer, Error, TEXT("Failed to register prompt: %s"), *Prompt.name);
		}
	}
	
	UE_LOG(LogUnrealMCPServer, Log, TEXT("Loaded %d prompts from prompts.json"), RegisteredCount);
	return RegisteredCount > 0;
}

void FUMCP_CommonPrompts::Register(class FUMCP_Server* Server)
{
	UE_LOG(LogUnrealMCPServer, Log, TEXT("Registering common MCP prompts."));

	// Try to load from JSON first
	if (LoadPromptsFromJson(Server))
	{
		UE_LOG(LogUnrealMCPServer, Log, TEXT("Successfully loaded prompts from JSON"));
		return;
	}
	
	// Fallback to hardcoded definitions if JSON loading fails
	UE_LOG(LogUnrealMCPServer, Warning, TEXT("Failed to load prompts from JSON, using hardcoded fallback"));
	
	// Register analyze_blueprint prompt
	{
		FUMCP_PromptDefinitionInternal Prompt;
		Prompt.name = TEXT("analyze_blueprint");
		Prompt.title = TEXT("Analyze Blueprint");
		Prompt.description = TEXT("Analyze a Blueprint's structure, functionality, and design patterns. Provides comprehensive analysis including variables, functions, events, graph structure, design patterns, dependencies, potential issues, and improvement suggestions.");
		
		FUMCP_PromptArgument Arg1;
		Arg1.name = TEXT("blueprint_path");
		Arg1.description = TEXT("The path to the Blueprint asset (e.g., '/Game/Blueprints/BP_Player')");
		Arg1.required = true;
		Prompt.arguments.Add(Arg1);
		
		FUMCP_PromptArgument Arg2;
		Arg2.name = TEXT("focus_areas");
		Arg2.description = TEXT("Comma-separated list of areas to focus on: 'variables', 'functions', 'events', 'graph', 'design', or 'all' (default: 'all')");
		Arg2.required = false;
		Prompt.arguments.Add(Arg2);
		
		Prompt.GetPrompt.BindRaw(this, &FUMCP_CommonPrompts::HandleAnalyzeBlueprintPrompt);
		
		if (Server->RegisterPrompt(MoveTemp(Prompt)))
		{
			UE_LOG(LogUnrealMCPServer, Log, TEXT("Registered analyze_blueprint prompt."));
		}
		else
		{
			UE_LOG(LogUnrealMCPServer, Error, TEXT("Failed to register analyze_blueprint prompt."));
		}
	}

	// Register refactor_blueprint prompt
	{
		FUMCP_PromptDefinitionInternal Prompt;
		Prompt.name = TEXT("refactor_blueprint");
		Prompt.title = TEXT("Refactor Blueprint");
		Prompt.description = TEXT("Generate a refactoring plan for a Blueprint. Provides current state analysis, refactoring strategy, step-by-step plan, breaking changes, testing plan, and migration guide.");
		
		FUMCP_PromptArgument Arg1;
		Arg1.name = TEXT("blueprint_path");
		Arg1.description = TEXT("The path to the Blueprint asset");
		Arg1.required = true;
		Prompt.arguments.Add(Arg1);
		
		FUMCP_PromptArgument Arg2;
		Arg2.name = TEXT("refactor_goal");
		Arg2.description = TEXT("The goal of the refactoring (e.g., 'improve performance', 'add new feature', 'simplify structure')");
		Arg2.required = true;
		Prompt.arguments.Add(Arg2);
		
		Prompt.GetPrompt.BindRaw(this, &FUMCP_CommonPrompts::HandleRefactorBlueprintPrompt);
		
		if (Server->RegisterPrompt(MoveTemp(Prompt)))
		{
			UE_LOG(LogUnrealMCPServer, Log, TEXT("Registered refactor_blueprint prompt."));
		}
		else
		{
			UE_LOG(LogUnrealMCPServer, Error, TEXT("Failed to register refactor_blueprint prompt."));
		}
	}

	// Register audit_assets prompt
	{
		FUMCP_PromptDefinitionInternal Prompt;
		Prompt.name = TEXT("audit_assets");
		Prompt.title = TEXT("Audit Assets");
		Prompt.description = TEXT("Audit project assets for dependencies, references, or issues. Provides asset inventory, dependency analysis, reference analysis, unused assets, orphaned assets, circular dependencies, and recommendations.");
		
		FUMCP_PromptArgument Arg1;
		Arg1.name = TEXT("asset_paths");
		Arg1.description = TEXT("Comma-separated list of asset paths to audit");
		Arg1.required = true;
		Prompt.arguments.Add(Arg1);
		
		FUMCP_PromptArgument Arg2;
		Arg2.name = TEXT("audit_type");
		Arg2.description = TEXT("Type of audit: 'dependencies', 'references', 'unused', 'orphaned', or 'all' (default: 'dependencies')");
		Arg2.required = false;
		Prompt.arguments.Add(Arg2);
		
		Prompt.GetPrompt.BindRaw(this, &FUMCP_CommonPrompts::HandleAuditAssetsPrompt);
		
		if (Server->RegisterPrompt(MoveTemp(Prompt)))
		{
			UE_LOG(LogUnrealMCPServer, Log, TEXT("Registered audit_assets prompt."));
		}
		else
		{
			UE_LOG(LogUnrealMCPServer, Error, TEXT("Failed to register audit_assets prompt."));
		}
	}

	// Register create_blueprint prompt
	{
		FUMCP_PromptDefinitionInternal Prompt;
		Prompt.name = TEXT("create_blueprint");
		Prompt.title = TEXT("Create Blueprint");
		Prompt.description = TEXT("Generate a design plan for creating a new Blueprint. Provides Blueprint structure, component requirements, initialization logic, core functionality, event handlers, dependencies, implementation steps, and testing checklist.");
		
		FUMCP_PromptArgument Arg1;
		Arg1.name = TEXT("blueprint_name");
		Arg1.description = TEXT("Name for the new Blueprint (e.g., 'BP_PlayerController')");
		Arg1.required = true;
		Prompt.arguments.Add(Arg1);
		
		FUMCP_PromptArgument Arg2;
		Arg2.name = TEXT("parent_class");
		Arg2.description = TEXT("Parent class to inherit from (e.g., 'PlayerController', 'Actor', 'Pawn')");
		Arg2.required = true;
		Prompt.arguments.Add(Arg2);
		
		FUMCP_PromptArgument Arg3;
		Arg3.name = TEXT("description");
		Arg3.description = TEXT("Description of what the Blueprint should do");
		Arg3.required = true;
		Prompt.arguments.Add(Arg3);
		
		Prompt.GetPrompt.BindRaw(this, &FUMCP_CommonPrompts::HandleCreateBlueprintPrompt);
		
		if (Server->RegisterPrompt(MoveTemp(Prompt)))
		{
			UE_LOG(LogUnrealMCPServer, Log, TEXT("Registered create_blueprint prompt."));
		}
		else
		{
			UE_LOG(LogUnrealMCPServer, Error, TEXT("Failed to register create_blueprint prompt."));
		}
	}

	// Register analyze_performance prompt
	{
		FUMCP_PromptDefinitionInternal Prompt;
		Prompt.name = TEXT("analyze_performance");
		Prompt.title = TEXT("Analyze Performance");
		Prompt.description = TEXT("Analyze the performance characteristics of a Blueprint. Identifies performance hotspots, tick analysis, memory usage, event frequency, optimization opportunities, best practices, and profiling recommendations.");
		
		FUMCP_PromptArgument Arg1;
		Arg1.name = TEXT("blueprint_path");
		Arg1.description = TEXT("The path to the Blueprint asset");
		Arg1.required = true;
		Prompt.arguments.Add(Arg1);
		
		Prompt.GetPrompt.BindRaw(this, &FUMCP_CommonPrompts::HandleAnalyzePerformancePrompt);
		
		if (Server->RegisterPrompt(MoveTemp(Prompt)))
		{
			UE_LOG(LogUnrealMCPServer, Log, TEXT("Registered analyze_performance prompt."));
		}
		else
		{
			UE_LOG(LogUnrealMCPServer, Error, TEXT("Failed to register analyze_performance prompt."));
		}
	}
}

FString FUMCP_CommonPrompts::FormatPromptTemplate(const FString& PromptName, TSharedPtr<FJsonObject> Arguments)
{
	FString* TemplatePtr = PromptTemplates.Find(PromptName);
	if (!TemplatePtr)
	{
		return FString(); // Template not found
	}
	
	FString Result = *TemplatePtr;
	
	// Replace all {argument_name} placeholders with actual values
	if (Arguments.IsValid())
	{
		// Get all field names from the arguments object
		TArray<FString> FieldNames;
		Arguments->Values.GetKeys(FieldNames);
		
		for (const FString& FieldName : FieldNames)
		{
			FString FieldValue;
			if (Arguments->TryGetStringField(FieldName, FieldValue))
			{
				FString Placeholder = FString::Printf(TEXT("{%s}"), *FieldName);
				Result.ReplaceInline(*Placeholder, *FieldValue);
			}
		}
	}
	
	return Result;
}

TArray<FUMCP_PromptMessage> FUMCP_CommonPrompts::HandleAnalyzeBlueprintPrompt(TSharedPtr<FJsonObject> Arguments)
{
	TArray<FUMCP_PromptMessage> Messages;
	
	FString BlueprintPath;
	FString FocusAreas = TEXT("all");
	
	if (Arguments.IsValid())
	{
		Arguments->TryGetStringField(TEXT("blueprint_path"), BlueprintPath);
		Arguments->TryGetStringField(TEXT("focus_areas"), FocusAreas);
	}
	
	// Try to use template from JSON, fallback to hardcoded if not found
	FString PromptText = FormatPromptTemplate(TEXT("analyze_blueprint"), Arguments);
	
	if (PromptText.IsEmpty())
	{
		// Fallback to hardcoded template
		PromptText = FString::Printf(TEXT(
			"Analyze the Blueprint at path '%s' and provide a comprehensive analysis.\n\n"
			"Focus Areas: %s\n\n"
			"Please provide:\n"
			"1. **Overview**: High-level description of what this Blueprint does\n"
			"2. **Variables**: List and explain all variables, their types, and purposes\n"
			"3. **Functions**: Document all custom functions, their parameters, return values, and logic\n"
			"4. **Events**: Identify all event handlers (BeginPlay, Tick, etc.) and their purposes\n"
			"5. **Graph Structure**: Describe the overall flow and key connections in the Blueprint graph\n"
			"6. **Design Patterns**: Identify any design patterns used (e.g., State Machine, Component Pattern)\n"
			"7. **Dependencies**: List assets and classes this Blueprint depends on\n"
			"8. **Potential Issues**: Identify any potential bugs, performance issues, or design concerns\n"
			"9. **Suggestions**: Provide recommendations for improvements or best practices\n\n"
			"Use the export_blueprint_markdown tool to get the full Blueprint structure, then analyze it thoroughly."
		), *BlueprintPath, *FocusAreas);
	}
	
	FUMCP_PromptMessage Message;
	Message.role = TEXT("user");
	Message.content = MakeShared<FJsonObject>();
	Message.content->SetStringField(TEXT("type"), TEXT("text"));
	Message.content->SetStringField(TEXT("text"), PromptText);
	
	Messages.Add(Message);
	return Messages;
}

TArray<FUMCP_PromptMessage> FUMCP_CommonPrompts::HandleRefactorBlueprintPrompt(TSharedPtr<FJsonObject> Arguments)
{
	TArray<FUMCP_PromptMessage> Messages;
	
	FString BlueprintPath;
	FString RefactorGoal;
	
	if (Arguments.IsValid())
	{
		Arguments->TryGetStringField(TEXT("blueprint_path"), BlueprintPath);
		Arguments->TryGetStringField(TEXT("refactor_goal"), RefactorGoal);
	}
	
	FString PromptText = FormatPromptTemplate(TEXT("refactor_blueprint"), Arguments);
	if (PromptText.IsEmpty())
	{
		// Fallback
		PromptText = FString::Printf(TEXT(
			"Create a refactoring plan for the Blueprint at '%s'.\n\n"
			"Refactoring Goal: %s\n\n"
			"Please provide:\n"
			"1. **Current State Analysis**: Analyze the current Blueprint structure\n"
			"2. **Refactoring Strategy**: Outline the approach to achieve the goal\n"
			"3. **Step-by-Step Plan**: Detailed steps for the refactoring\n"
			"4. **Breaking Changes**: Identify any breaking changes that might affect other assets\n"
			"5. **Testing Plan**: Suggest how to test the refactored Blueprint\n"
			"6. **Migration Guide**: If applicable, provide a guide for migrating dependent assets\n\n"
			"Use the export_blueprint_markdown tool to examine the current Blueprint structure."
		), *BlueprintPath, *RefactorGoal);
	}
	
	FUMCP_PromptMessage Message;
	Message.role = TEXT("user");
	Message.content = MakeShared<FJsonObject>();
	Message.content->SetStringField(TEXT("type"), TEXT("text"));
	Message.content->SetStringField(TEXT("text"), PromptText);
	
	Messages.Add(Message);
	return Messages;
}

TArray<FUMCP_PromptMessage> FUMCP_CommonPrompts::HandleAuditAssetsPrompt(TSharedPtr<FJsonObject> Arguments)
{
	TArray<FUMCP_PromptMessage> Messages;
	
	FString AssetPaths;
	FString AuditType = TEXT("dependencies");
	
	if (Arguments.IsValid())
	{
		Arguments->TryGetStringField(TEXT("asset_paths"), AssetPaths);
		Arguments->TryGetStringField(TEXT("audit_type"), AuditType);
	}
	
	FString PromptText = FormatPromptTemplate(TEXT("audit_assets"), Arguments);
	if (PromptText.IsEmpty())
	{
		// Fallback
		PromptText = FString::Printf(TEXT(
			"Audit the following assets: %s\n\n"
			"Audit Type: %s\n\n"
			"Please provide:\n"
			"1. **Asset Inventory**: List all assets and their basic information\n"
			"2. **Dependency Analysis**: Map dependencies between assets (use get_asset_dependencies tool)\n"
			"3. **Reference Analysis**: Identify what references each asset (use get_asset_references tool)\n"
			"4. **Unused Assets**: Identify assets that are not referenced by any other asset\n"
			"5. **Orphaned Assets**: Find assets with broken or missing dependencies\n"
			"6. **Circular Dependencies**: Detect any circular dependency chains\n"
			"7. **Recommendations**: Suggest optimizations, cleanup opportunities, or restructuring\n\n"
			"Use the search_assets, get_asset_dependencies, and get_asset_references tools to gather information."
		), *AssetPaths, *AuditType);
	}
	
	FUMCP_PromptMessage Message;
	Message.role = TEXT("user");
	Message.content = MakeShared<FJsonObject>();
	Message.content->SetStringField(TEXT("type"), TEXT("text"));
	Message.content->SetStringField(TEXT("text"), PromptText);
	
	Messages.Add(Message);
	return Messages;
}

TArray<FUMCP_PromptMessage> FUMCP_CommonPrompts::HandleCreateBlueprintPrompt(TSharedPtr<FJsonObject> Arguments)
{
	TArray<FUMCP_PromptMessage> Messages;
	
	FString BlueprintName;
	FString ParentClass;
	FString Description;
	
	if (Arguments.IsValid())
	{
		Arguments->TryGetStringField(TEXT("blueprint_name"), BlueprintName);
		Arguments->TryGetStringField(TEXT("parent_class"), ParentClass);
		Arguments->TryGetStringField(TEXT("description"), Description);
	}
	
	FString PromptText = FormatPromptTemplate(TEXT("create_blueprint"), Arguments);
	if (PromptText.IsEmpty())
	{
		// Fallback
		PromptText = FString::Printf(TEXT(
			"Create a design plan for a new Blueprint named '%s' that inherits from '%s'.\n\n"
			"Description: %s\n\n"
			"Please provide:\n"
			"1. **Blueprint Structure**: Define the variables, functions, and events needed\n"
			"2. **Component Requirements**: List any components that should be added\n"
			"3. **Initialization Logic**: Outline what should happen in BeginPlay and construction\n"
			"4. **Core Functionality**: Describe the main functions and their implementations\n"
			"5. **Event Handlers**: Specify which events to handle and how\n"
			"6. **Dependencies**: Identify other assets or classes this Blueprint will need\n"
			"7. **Implementation Steps**: Step-by-step guide for creating the Blueprint in Unreal Editor\n"
			"8. **Testing Checklist**: Items to test once the Blueprint is created\n\n"
			"Use search_blueprints to find similar existing Blueprints for reference."
		), *BlueprintName, *ParentClass, *Description);
	}
	
	FUMCP_PromptMessage Message;
	Message.role = TEXT("user");
	Message.content = MakeShared<FJsonObject>();
	Message.content->SetStringField(TEXT("type"), TEXT("text"));
	Message.content->SetStringField(TEXT("text"), PromptText);
	
	Messages.Add(Message);
	return Messages;
}

TArray<FUMCP_PromptMessage> FUMCP_CommonPrompts::HandleAnalyzePerformancePrompt(TSharedPtr<FJsonObject> Arguments)
{
	TArray<FUMCP_PromptMessage> Messages;
	
	FString BlueprintPath;
	
	if (Arguments.IsValid())
	{
		Arguments->TryGetStringField(TEXT("blueprint_path"), BlueprintPath);
	}
	
	FString PromptText = FormatPromptTemplate(TEXT("analyze_performance"), Arguments);
	if (PromptText.IsEmpty())
	{
		// Fallback
		PromptText = FString::Printf(TEXT(
			"Analyze the performance of the Blueprint at '%s'.\n\n"
			"Please provide:\n"
			"1. **Performance Hotspots**: Identify nodes or functions that might cause performance issues\n"
			"2. **Tick Analysis**: Review Tick event usage and suggest optimizations\n"
			"3. **Memory Usage**: Analyze variable usage and memory footprint\n"
			"4. **Event Frequency**: Identify frequently called events and their impact\n"
			"5. **Optimization Opportunities**: Suggest specific optimizations (e.g., caching, batching, reducing tick frequency)\n"
			"6. **Best Practices**: Recommend performance best practices for this Blueprint\n"
			"7. **Profiling Recommendations**: Suggest what to profile in Unreal's profiler\n\n"
			"Use export_blueprint_markdown to examine the Blueprint structure, then analyze it for performance concerns."
		), *BlueprintPath);
	}
	
	FUMCP_PromptMessage Message;
	Message.role = TEXT("user");
	Message.content = MakeShared<FJsonObject>();
	Message.content->SetStringField(TEXT("type"), TEXT("text"));
	Message.content->SetStringField(TEXT("text"), PromptText);
	
	Messages.Add(Message);
	return Messages;
}

