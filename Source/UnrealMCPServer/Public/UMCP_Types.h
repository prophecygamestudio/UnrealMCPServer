#pragma once

// NOTE: Property Naming Convention
// All USTRUCT properties in this file use camelCase naming (e.g., "searchType", "objectPath") 
// instead of Unreal Engine's standard PascalCase convention (e.g., "SearchType", "ObjectPath").
// This is intentional and done to align with web/JSON standards where camelCase is the common convention.
// While this breaks Unreal Engine coding standards, it is acceptable here because:
// 1. These structures are primarily used for external API communication (MCP protocol)
// 2. The JSON serialization uses EJsonObjectConversionFlags which standardizes case, converting
//    PascalCase to camelCase by default unless SkipStandardizeCase is passed
// 3. Web clients expect camelCase in JSON APIs, making this the more practical choice
// 4. The external tooling ecosystem (MCP clients) follows web standards, not engine standards

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Templates/SharedPointer.h"
#include "JsonUtilities.h"
#include "Serialization/JsonSerializer.h"
#include "UMCP_UriTemplate.h"
#include "UMCP_Types.generated.h"

// Standard JSON-RPC 2.0 Error Codes & MCP Specific Codes
enum class EUMCP_JsonRpcErrorCode : int32
{
    // Standard JSON-RPC 2.0 Error Codes
    ParseError = -32700,
	ResourceNotFound = -32002,
    InvalidRequest = -32600,
    MethodNotFound = -32601,
    InvalidParams = -32602,
    InternalError = -32603,
    ServerError = -32000, // Generic server error base

    // -32000 to -32099 are reserved for implementation-defined server-errors.
    // We can add more specific server errors in this range if needed.

    // MCP Specific Error Codes (can start from -32099 downwards or use another range)
    // Example: OperationNotSupported = -32001,
};

// Represents a JSON-RPC Request ID, which can be a string, number, or null.
// Also handles the concept of an "absent" ID for notifications that don't send one.
USTRUCT()
struct FUMCP_JsonRpcId
{
    GENERATED_BODY()

private:
    // Internal storage for the ID. Can be FJsonValueString, FJsonValueNumber, FJsonValueNull, or nullptr (for absent ID).
    TSharedPtr<FJsonValue> Value;

    // Private constructor for internal use by static factories or specific type constructors
    FUMCP_JsonRpcId(TSharedPtr<FJsonValue> InValue) : Value(InValue) {}

public:
    // Default constructor: Represents an absent ID (e.g. for notifications or when ID field is missing).
    FUMCP_JsonRpcId() : Value(nullptr) {}
    FUMCP_JsonRpcId(const FString& InString) : Value(MakeShared<FJsonValueString>(InString)) {}
    FUMCP_JsonRpcId(int32 InNumber) : Value(MakeShared<FJsonValueNumber>(static_cast<double>(InNumber))) {}

    static FUMCP_JsonRpcId CreateNullId();
    static FUMCP_JsonRpcId CreateFromJsonValue(const TSharedPtr<FJsonValue>& JsonValue);
    bool IsString() const;
    bool IsNumber() const;
    bool IsNull() const;

    TSharedPtr<FJsonValue> GetJsonValue() const;
    FString ToString() const;
};

// Forward declaration
struct FUMCP_JsonRpcError;

USTRUCT()
struct FUMCP_JsonRpcRequest
{
    GENERATED_BODY()

    UPROPERTY()
    FString jsonrpc; // Should be "2.0"

    UPROPERTY()
    FString method;

    TSharedPtr<FJsonObject> params; // Using TSharedPtr<FJsonObject> for params
    FUMCP_JsonRpcId id;

    FUMCP_JsonRpcRequest() : jsonrpc(TEXT("2.0")) {}
    bool ToJsonString(FString& OutJsonString) const;
    static bool CreateFromJsonString(const FString& JsonString, FUMCP_JsonRpcRequest& OutRequest);
};

USTRUCT()
struct FUMCP_JsonRpcError
{
    GENERATED_BODY()

    UPROPERTY()
    int32 code;

    UPROPERTY()
    FString message;

    // data can be any JSON value, using FJsonValue to represent it.
    TSharedPtr<FJsonValue> data; 

    FUMCP_JsonRpcError() : code(0) {}
    FUMCP_JsonRpcError(EUMCP_JsonRpcErrorCode InErrorCode, const FString& InMessage, TSharedPtr<FJsonValue> InData = nullptr)
        : code(static_cast<int32>(InErrorCode)), message(InMessage), data(InData) {}
	void SetError(EUMCP_JsonRpcErrorCode error) { code = static_cast<int32>(error); }

    bool ToJsonObject(TSharedPtr<FJsonObject>& OutJsonObject) const;
    static bool CreateFromJsonObject(const TSharedPtr<FJsonObject>& JsonObject, FUMCP_JsonRpcError& OutErrorObject);
};

USTRUCT()
struct FUMCP_JsonRpcResponse
{
    GENERATED_BODY()

    UPROPERTY()
    FString jsonrpc; // Should be "2.0"

    // ID is now FJsonRpcId to encapsulate its type (string, number, null, or absent)
    FUMCP_JsonRpcId id;

    // Result can be any valid JSON value (object, array, string, number, boolean, null)
    TSharedPtr<FJsonValue> result; 
    
    TSharedPtr<FUMCP_JsonRpcError> error; // Error object if an error occurred

    FUMCP_JsonRpcResponse() : jsonrpc(TEXT("2.0")) {}

    bool ToJsonString(FString& OutJsonString) const;
    static bool CreateFromJsonString(const FString& JsonString, FUMCP_JsonRpcResponse& OutResponse);
};

// MCP Specific Structures
template<typename T>
UNREALMCPSERVER_API bool UMCP_ToJsonObject(const T& InStruct, TSharedPtr<FJsonObject>& OutJsonObject)
{
	if (!OutJsonObject.IsValid()) return false;
	return FJsonObjectConverter::UStructToJsonObject(InStruct.StaticStruct(), &InStruct, OutJsonObject.ToSharedRef());
}

// Helper function to convert USTRUCT directly to JSON string
// This is the common pattern used throughout the codebase
template<typename T>
UNREALMCPSERVER_API bool UMCP_ToJsonString(const T& InStruct, FString& OutJsonString)
{
	TSharedPtr<FJsonObject> JsonObject = MakeShared<FJsonObject>();
	if (!UMCP_ToJsonObject(InStruct, JsonObject))
	{
		return false;
	}
	
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutJsonString);
	return FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);
}

template<typename T>
UNREALMCPSERVER_API bool UMCP_CreateFromJsonObject(const TSharedPtr<FJsonObject>& JsonObject, T& OutStruct, bool bAllowMissingObject = false)
{
	if (!JsonObject.IsValid()) return bAllowMissingObject;
	return FJsonObjectConverter::JsonObjectToUStruct(JsonObject.ToSharedRef(), &OutStruct, 0, 0);
}

USTRUCT()
struct FUMCP_ServerInfo
{
    GENERATED_BODY()

    UPROPERTY()
    FString name;

    UPROPERTY()
    FString version;

    FUMCP_ServerInfo() : name(TEXT("UnrealMCPServer")) {}
};

USTRUCT()
struct FUMCP_ServerCapabilitiesTools
{
    GENERATED_BODY()

    UPROPERTY()
    bool listChanged = false; // Deferred as SSE is deferred

    UPROPERTY()
    bool inputSchema = true; // Whether server supports 'inputSchema' in ToolDefinition

    UPROPERTY()
    bool outputSchema = true; // Whether server supports 'outputSchema' in ToolDefinition
};

USTRUCT()
struct FUMCP_ServerCapabilitiesResources
{
    GENERATED_BODY()

    UPROPERTY()
    bool listChanged = false; // Deferred as SSE is deferred

    UPROPERTY()
    bool subscribe = false; // Deferred as SSE is deferred
};

USTRUCT()
struct FUMCP_ServerCapabilitiesPrompts
{
    GENERATED_BODY()

    UPROPERTY()
    bool listChanged = false; // Deferred as SSE is deferred
};

USTRUCT()
struct FUMCP_ServerCapabilities // Add more capability structs as needed (e.g., roots, sampling)
{
    GENERATED_BODY()

	//UPROPERTY()
	//FUMCP_ServerCapabilitiesLogging logging;

    UPROPERTY()
    FUMCP_ServerCapabilitiesTools tools;

    UPROPERTY()
    FUMCP_ServerCapabilitiesResources resources;

    UPROPERTY()
    FUMCP_ServerCapabilitiesPrompts prompts;
};

USTRUCT()
struct FUMCP_InitializeParams
{
    GENERATED_BODY()

    UPROPERTY()
    FString protocolVersion; // Client's supported protocol version, e.g., "2025-03-26"

    // Client capabilities can be added here later if needed for negotiation
    // UPROPERTY()
    // TSharedPtr<FClientCapabilities> capabilities;
	
    // Client capabilities can be added here later if needed for negotiation
    // UPROPERTY()
    // TSharedPtr<FJsonObject> clientInfo;
};

USTRUCT()
struct FUMCP_InitializeResult
{
    GENERATED_BODY()

    UPROPERTY()
    FString protocolVersion; // Server's chosen protocol version

    UPROPERTY()
    FUMCP_ServerInfo serverInfo;

    UPROPERTY()
    FUMCP_ServerCapabilities capabilities;
};

USTRUCT()
struct FUMCP_CallToolParams
{
	GENERATED_BODY()

	UPROPERTY()
	FString name;

	TSharedPtr<FJsonObject> arguments;
};

template<>
FORCEINLINE bool UMCP_CreateFromJsonObject<FUMCP_CallToolParams>(const TSharedPtr<FJsonObject>& JsonObject, FUMCP_CallToolParams& OutStruct, bool bAllowMissingObject)
{
	if (!JsonObject.IsValid()) return bAllowMissingObject;
	OutStruct.name = JsonObject->GetStringField(TEXT("name"));
	OutStruct.arguments = JsonObject->GetObjectField(TEXT("arguments"));
	return true;
}

USTRUCT()
struct UNREALMCPSERVER_API FUMCP_CallToolResultContent
{
	GENERATED_BODY()
	
	UPROPERTY()
	FString data; // used by `audio` and `image` types

	UPROPERTY()
	FString text; // used by `text` type

	UPROPERTY()
	FString mimetype; // used by `audio` and `image` types

	UPROPERTY()
	FString type;

	//TODO add embedded resource
};

USTRUCT()
struct FUMCP_CallToolResult
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FUMCP_CallToolResultContent> content;

	UPROPERTY()
	bool isError = false;
};

USTRUCT()
struct FUMCP_ListToolsParams
{
	GENERATED_BODY()

	UPROPERTY()
	FString cursor;
};

DECLARE_DELEGATE_RetVal_TwoParams(bool, FUMCP_ToolCall, TSharedPtr<FJsonObject> /* arguments */, TArray<FUMCP_CallToolResultContent>& /* OutContent */);

USTRUCT()
struct UNREALMCPSERVER_API FUMCP_ToolDefinition
{
	GENERATED_BODY()

	UPROPERTY()
	FString name;

	UPROPERTY()
	FString description;

	TSharedPtr<FJsonObject> inputSchema;
	TSharedPtr<FJsonObject> outputSchema; // Optional output schema for tools with well-known output formats
	FUMCP_ToolCall DoToolCall;

	FUMCP_ToolDefinition(): name{}, description{}, inputSchema{ MakeShared<FJsonObject>() }, outputSchema{ nullptr }, DoToolCall()
	{
		inputSchema->SetStringField(TEXT("type"), TEXT("object"));
	}
};

USTRUCT()
struct FUMCP_ListToolsResult
{
	GENERATED_BODY()

	UPROPERTY()
	FString nextCursor;

	UPROPERTY()
	TArray<FUMCP_ToolDefinition> tools;
};

USTRUCT()
struct FUMCP_ReadResourceParams
{
	GENERATED_BODY()

	UPROPERTY()
	FString uri;
};

USTRUCT()
struct FUMCP_ReadResourceResultContent
{
	GENERATED_BODY()

	UPROPERTY()
	FString uri;
	
	UPROPERTY()
	FString text; // Used by text resources
	
	UPROPERTY()
	FString blob; // Used by blob resources
	
	UPROPERTY()
	FString mimeType;
};

USTRUCT()
struct FUMCP_ReadResourceResult
{
	GENERATED_BODY()
	
	UPROPERTY()
	TArray<FUMCP_ReadResourceResultContent> contents;
};

USTRUCT()
struct FUMCP_ListResourcesParams
{
	GENERATED_BODY()

	UPROPERTY()
	FString cursor;
};

DECLARE_DELEGATE_RetVal_TwoParams(bool, FUMCP_ResourceRead, const FString& /* Uri */, TArray<FUMCP_ReadResourceResultContent>& /* OutContent */);

USTRUCT()
struct UNREALMCPSERVER_API FUMCP_ResourceDefinition
{
	GENERATED_BODY()

	UPROPERTY()
	FString name;

	UPROPERTY()
	FString description;

	UPROPERTY()
	FString mimeType;

	UPROPERTY()
	FString uri;

	// Part of the spec, but we don't need it for templated resources
	UPROPERTY()
	int32 size = 0;

	FUMCP_ResourceRead ReadResource;
};

USTRUCT()
struct UNREALMCPSERVER_API FUMCP_ListResourcesResult
{
	GENERATED_BODY()

	UPROPERTY()
	FString nextCursor;

	UPROPERTY()
	TArray<FUMCP_ResourceDefinition> resources;
};

USTRUCT()
struct FUMCP_ListResourceTemplatesParams
{
	GENERATED_BODY()

	UPROPERTY()
	FString cursor;
};

DECLARE_DELEGATE_RetVal_ThreeParams(bool, FUMCP_ResourceTemplateRead, const FUMCP_UriTemplate& /* Template */, const FUMCP_UriTemplateMatch& /* UriMatch */, TArray<FUMCP_ReadResourceResultContent>& /* OutContent */);

USTRUCT()
struct FUMCP_ResourceTemplateDefinition
{
	GENERATED_BODY()

	UPROPERTY()
	FString name;

	UPROPERTY()
	FString description;

	UPROPERTY()
	FString mimeType;

	UPROPERTY()
	FString uriTemplate;

	FUMCP_ResourceTemplateRead ReadResource;
};

USTRUCT()
struct FUMCP_ListResourceTemplatesResult
{
	GENERATED_BODY()

	UPROPERTY()
	FString nextCursor;

	UPROPERTY()
	TArray<FUMCP_ResourceTemplateDefinition> resourceTemplates;
};

// Prompt-related USTRUCTs

USTRUCT()
struct UNREALMCPSERVER_API FUMCP_PromptArgument
{
	GENERATED_BODY()

	UPROPERTY()
	FString name;

	UPROPERTY()
	FString description;

	UPROPERTY()
	bool required = false;
};

USTRUCT()
struct UNREALMCPSERVER_API FUMCP_PromptDefinition
{
	GENERATED_BODY()

	UPROPERTY()
	FString name;

	UPROPERTY()
	FString title; // Optional human-readable name

	UPROPERTY()
	FString description;

	UPROPERTY()
	TArray<FUMCP_PromptArgument> arguments;
};

USTRUCT()
struct FUMCP_ListPromptsParams
{
	GENERATED_BODY()

	UPROPERTY()
	FString cursor;
};

USTRUCT()
struct FUMCP_ListPromptsResult
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FUMCP_PromptDefinition> prompts;

	UPROPERTY()
	FString nextCursor;
};

USTRUCT()
struct FUMCP_GetPromptParams
{
	GENERATED_BODY()

	UPROPERTY()
	FString name;

	TSharedPtr<FJsonObject> arguments; // Optional arguments object
};

template<>
FORCEINLINE bool UMCP_CreateFromJsonObject<FUMCP_GetPromptParams>(const TSharedPtr<FJsonObject>& JsonObject, FUMCP_GetPromptParams& OutStruct, bool bAllowMissingObject)
{
	if (!JsonObject.IsValid()) return bAllowMissingObject;
	OutStruct.name = JsonObject->GetStringField(TEXT("name"));
	if (JsonObject->HasTypedField<EJson::Object>(TEXT("arguments")))
	{
		OutStruct.arguments = JsonObject->GetObjectField(TEXT("arguments"));
	}
	return true;
}

USTRUCT()
struct UNREALMCPSERVER_API FUMCP_PromptContent
{
	GENERATED_BODY()

	UPROPERTY()
	FString type; // "text", "image", "audio", or "resource"

	// For text content
	UPROPERTY()
	FString text;

	// For image/audio content
	UPROPERTY()
	FString data; // base64-encoded

	UPROPERTY()
	FString mimeType;

	// For resource content
	TSharedPtr<FJsonObject> resource; // Contains uri, mimeType, text/blob
};

USTRUCT()
struct UNREALMCPSERVER_API FUMCP_PromptMessage
{
	GENERATED_BODY()

	UPROPERTY()
	FString role; // "user" or "assistant"

	TSharedPtr<FJsonObject> content; // PromptContent as JSON object
};

USTRUCT()
struct FUMCP_GetPromptResult
{
	GENERATED_BODY()

	UPROPERTY()
	FString description;

	UPROPERTY()
	TArray<FUMCP_PromptMessage> messages;
};

DECLARE_DELEGATE_RetVal_OneParam(TArray<FUMCP_PromptMessage>, FUMCP_PromptGet, TSharedPtr<FJsonObject> /* arguments */);

USTRUCT()
struct UNREALMCPSERVER_API FUMCP_PromptDefinitionInternal
{
	GENERATED_BODY()

	UPROPERTY()
	FString name;

	UPROPERTY()
	FString title;

	UPROPERTY()
	FString description;

	UPROPERTY()
	TArray<FUMCP_PromptArgument> arguments;

	FUMCP_PromptGet GetPrompt;
};

// Tool Parameter USTRUCTs for type-safe JSON Schema generation

// SearchBlueprints tool parameters
USTRUCT()
struct UNREALMCPSERVER_API FUMCP_SearchBlueprintsParams
{
	GENERATED_BODY()

	UPROPERTY()
	FString searchType;

	UPROPERTY()
	FString searchTerm;

	UPROPERTY()
	FString packagePath;

	UPROPERTY()
	bool bRecursive = true;
};

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
};

// GetProjectConfig tool parameters
USTRUCT()
struct UNREALMCPSERVER_API FUMCP_GetProjectConfigParams
{
	GENERATED_BODY()
};

// ExecuteConsoleCommand tool parameters
USTRUCT()
struct UNREALMCPSERVER_API FUMCP_ExecuteConsoleCommandParams
{
	GENERATED_BODY()

	UPROPERTY()
	FString command;
};

// GetLogFilePath tool parameters
USTRUCT()
struct UNREALMCPSERVER_API FUMCP_GetLogFilePathParams
{
	GENERATED_BODY()
};

// Tool Output USTRUCTs
// Note: SearchBlueprints and SearchAssets outputs have complex nested structures
// that are built manually, so they don't have USTRUCT result types

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

// GetProjectConfig output
USTRUCT()
struct UNREALMCPSERVER_API FUMCP_EngineVersionInfo
{
	GENERATED_BODY()

	UPROPERTY()
	FString full;

	UPROPERTY()
	int32 major;

	UPROPERTY()
	int32 minor;

	UPROPERTY()
	int32 patch;

	UPROPERTY()
	int32 changelist;
};

USTRUCT()
struct UNREALMCPSERVER_API FUMCP_ProjectPaths
{
	GENERATED_BODY()

	UPROPERTY()
	FString engineDir;

	UPROPERTY()
	FString projectDir;

	UPROPERTY()
	FString projectContentDir;

	UPROPERTY()
	FString projectLogDir;

	UPROPERTY()
	FString projectSavedDir;

	UPROPERTY()
	FString projectConfigDir;

	UPROPERTY()
	FString projectPluginsDir;

	UPROPERTY()
	FString engineContentDir;

	UPROPERTY()
	FString enginePluginsDir;
};

USTRUCT()
struct UNREALMCPSERVER_API FUMCP_GetProjectConfigResult
{
	GENERATED_BODY()

	UPROPERTY()
	FUMCP_EngineVersionInfo engineVersion;

	UPROPERTY()
	FUMCP_ProjectPaths paths;
};

// ExecuteConsoleCommand output
USTRUCT()
struct UNREALMCPSERVER_API FUMCP_ExecuteConsoleCommandResult
{
	GENERATED_BODY()

	UPROPERTY()
	bool bSuccess;

	UPROPERTY()
	FString command;

	UPROPERTY()
	FString output;

	UPROPERTY()
	FString error;
};

// GetLogFilePath output
USTRUCT()
struct UNREALMCPSERVER_API FUMCP_GetLogFilePathResult
{
	GENERATED_BODY()

	UPROPERTY()
	FString logFilePath;
};

// Helper function to generate JSON Schema from USTRUCT
// PropertyDescriptions: Map of property names to their descriptions
// RequiredFields: Array of property names that are required
// EnumValues: Map of property names to their enum values (for string enums)
UNREALMCPSERVER_API TSharedPtr<FJsonObject> UMCP_GenerateJsonSchemaFromStruct(
	const UScriptStruct* Struct,
	const TMap<FString, FString>& PropertyDescriptions = TMap<FString, FString>(),
	const TArray<FString>& RequiredFields = TArray<FString>(),
	const TMap<FString, TArray<FString>>& EnumValues = TMap<FString, TArray<FString>>()
);
