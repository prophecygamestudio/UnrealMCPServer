#pragma once

#include "UMCP_Types.h"

// Forward declarations
class FUMCP_Server;
struct FUMCP_ReadResourceResultContent; // Forward declare for the delegate parameter

/**
 * Handles requests for common Unreal Engine resources.
 */
class FUMCP_CommonResources
{
public:
	void Register(class FUMCP_Server* Server);

private:
	/**
	 * Get the path to the Resources directory containing JSON definitions.
	 */
	FString GetResourcesPath();
	
	/**
	 * Load resource templates from resources.json file.
	 * Returns true if successful, false otherwise.
	 */
	bool LoadResourcesFromJson(class FUMCP_Server* Server);
	/**
	 * Handles requests for T3D representation of Unreal Engine Blueprints via a templated URI.
	 * This is bound to FUMCP_ResourceTemplateDefinition::ReadResource.
	 * URI scheme: unreal+t3d://{filepath}
	 */
	bool HandleT3DResourceRequest(const FUMCP_UriTemplate& UriTemplate, const FUMCP_UriTemplateMatch& Match, TArray<FUMCP_ReadResourceResultContent>& OutContent);

	/**
	 * Handles requests for Markdown representation of Unreal Engine Blueprints via a templated URI.
	 * This is bound to FUMCP_ResourceTemplateDefinition::ReadResource.
	 * URI scheme: unreal+md://{filepath}
	 */
	bool HandleMarkdownResourceRequest(const FUMCP_UriTemplate& UriTemplate, const FUMCP_UriTemplateMatch& Match, TArray<FUMCP_ReadResourceResultContent>& OutContent);
};
