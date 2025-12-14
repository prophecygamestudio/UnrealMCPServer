#pragma once

#include "UMCP_Types.h"

// Forward declarations
class FUMCP_Server;

/**
 * Handles registration of common prompts for Unreal Engine workflows.
 */
class FUMCP_CommonPrompts
{
public:
	void Register(class FUMCP_Server* Server);

private:
	/**
	 * Get the path to the Resources directory containing JSON definitions.
	 */
	FString GetResourcesPath();
	
	/**
	 * Load prompts from prompts.json file.
	 * Returns true if successful, false otherwise.
	 */
	bool LoadPromptsFromJson(class FUMCP_Server* Server);
	
	/**
	 * Map of prompt names to their templates (loaded from JSON).
	 */
	TMap<FString, FString> PromptTemplates;
	
	/**
	 * Format a prompt template with arguments.
	 * Replaces {argument_name} placeholders with actual values.
	 */
	FString FormatPromptTemplate(const FString& PromptName, TSharedPtr<FJsonObject> Arguments);
	/**
	 * Handler for analyze_blueprint prompt.
	 * Generates a prompt for analyzing Blueprint structure and functionality.
	 */
	TArray<FUMCP_PromptMessage> HandleAnalyzeBlueprintPrompt(TSharedPtr<FJsonObject> Arguments);

	/**
	 * Handler for refactor_blueprint prompt.
	 * Generates a prompt for refactoring Blueprints.
	 */
	TArray<FUMCP_PromptMessage> HandleRefactorBlueprintPrompt(TSharedPtr<FJsonObject> Arguments);

	/**
	 * Handler for audit_assets prompt.
	 * Generates a prompt for auditing project assets.
	 */
	TArray<FUMCP_PromptMessage> HandleAuditAssetsPrompt(TSharedPtr<FJsonObject> Arguments);

	/**
	 * Handler for create_blueprint prompt.
	 * Generates a prompt for creating new Blueprints.
	 */
	TArray<FUMCP_PromptMessage> HandleCreateBlueprintPrompt(TSharedPtr<FJsonObject> Arguments);

	/**
	 * Handler for analyze_performance prompt.
	 * Generates a prompt for analyzing Blueprint performance.
	 */
	TArray<FUMCP_PromptMessage> HandleAnalyzePerformancePrompt(TSharedPtr<FJsonObject> Arguments);
};

