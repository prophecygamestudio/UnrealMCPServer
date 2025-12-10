#include "UMCP_Server.h"
#include "UMCP_Types.h"
#include "UMCP_CommonTools.h"
#include "UMCP_CommonResources.h"
#include "UnrealMCPServerModule.h"

#include "HttpServerModule.h"
#include "IHttpRouter.h"
#include "HttpServerResponse.h"
#include "JsonUtilities.h"
#include "Json.h"
#include "UMCP_UriTemplate.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Engine/Engine.h"
#include "Async/Async.h"


const FString FUMCP_Server::MCP_PROTOCOL_VERSION = TEXT("2024-11-05");//TEXT("2025-03-26");
const FString FUMCP_Server::PLUGIN_VERSION = TEXT("0.1.0");

void FUMCP_Server::StartServer()
{
	FHttpServerModule& HttpServerModule = FHttpServerModule::Get();
	HttpRouter = HttpServerModule.GetHttpRouter(HttpServerPort);
	if (!HttpRouter.IsValid())
	{
		UE_LOG(LogUnrealMCPServer, Error, TEXT("Failed to get HttpRouter on port %d. Another server might be running or port is in use."), HttpServerPort);
		return;
	}

	RouteHandle_MCPStreamableHTTP = HttpRouter->BindRoute(FHttpPath(TEXT("/mcp")), EHttpServerRequestVerbs::VERB_POST,
#if (ENGINE_MAJOR_VERSION >= (5) && ENGINE_MINOR_VERSION >= (4))
		FHttpRequestHandler::CreateLambda(
#endif
			[this](const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete) -> bool
			{
				AsyncTask(ENamedThreads::GameThread, [this, Request, OnComplete]() {
					this->HandleStreamableHTTPMCPRequest(Request, OnComplete);
				});
				return true;
			}
#if (ENGINE_MAJOR_VERSION == (5) && ENGINE_MINOR_VERSION >= (5))
		)
#endif
		);
	RegisterInternalRpcMethodHandlers();

	UE_LOG(LogUnrealMCPServer, Log, TEXT("Bound /mcp to handler."));

    // Start listening for requests
    HttpServerModule.StartAllListeners();
	UE_LOG(LogUnrealMCPServer, Log, TEXT("HTTP Server started on port %d"), HttpServerPort);
}

void FUMCP_Server::StopServer()
{
	if (HttpRouter.IsValid())
	{
		// Unbind routes
		if (RouteHandle_MCPStreamableHTTP.IsValid())
		{
			HttpRouter->UnbindRoute(RouteHandle_MCPStreamableHTTP);
			RouteHandle_MCPStreamableHTTP.Reset();
		}

		UE_LOG(LogUnrealMCPServer, Log, TEXT("All routes unbound."));
		HttpRouter.Reset();
	}
	JsonRpcMethodHandlers.Empty();

	// let the HttpServerModule clean itself up whenever it goes away.  Nothing to do here
}

void FUMCP_Server::RegisterRpcMethodHandler(const FString& MethodName, UMCP_JsonRpcHandler&& Handler)
{
	JsonRpcMethodHandlers.Add(MethodName, Handler);
}

bool FUMCP_Server::RegisterTool(FUMCP_ToolDefinition Tool)
{
	if (!Tool.DoToolCall.IsBound())
	{
		return false;
	}
	if (Tools.Contains(Tool.name))
	{
		return false;
	}
	Tools.Add(Tool.name, Tool);
	return true;
}

bool FUMCP_Server::RegisterResource(FUMCP_ResourceDefinition Resource)
{
	if (!Resource.ReadResource.IsBound())
	{
		return false;
	}
	if (Resources.Contains(Resource.uri))
	{
		return false;
	}
	Resources.Add(Resource.uri, Resource);
	return true;
}

bool FUMCP_Server::RegisterResourceTemplate(FUMCP_ResourceTemplateDefinition ResourceTemplate)
{
	if (!ResourceTemplate.ReadResource.IsBound())
	{
		return false;
	}
	FUMCP_UriTemplate UriTemplate{ResourceTemplate.uriTemplate};
	if (!UriTemplate.IsValid())
	{
		return false;
	}
	
	ResourceTemplates.Emplace(MoveTemp(UriTemplate), MoveTemp(ResourceTemplate));
	return true;
}

bool FUMCP_Server::RegisterPrompt(FUMCP_PromptDefinitionInternal Prompt)
{
	if (!Prompt.GetPrompt.IsBound())
	{
		return false;
	}
	if (Prompts.Contains(Prompt.name))
	{
		return false;
	}
	Prompts.Add(Prompt.name, Prompt);
	return true;
}

// Helper to send a JSON response
void FUMCP_Server::SendJsonRpcResponse(const FHttpResultCallback& OnComplete, const FUMCP_JsonRpcResponse& RpcResponse)
{
	FString JsonPayload;
	if (!RpcResponse.ToJsonString(JsonPayload))
	{
		UE_LOG(LogUnrealMCPServer, Error, TEXT("Failed to serialize response."));
		JsonPayload = TEXT("{\"jsonrpc\": \"2.0\", \"id\": null, \"error\": {\"code\": -32603, \"message\": \"Internal error - Failed to serialize response\"}}");
	}

	if (JsonPayload.Len() > 1000)
	{
		UE_LOG(LogUnrealMCPServer, Verbose, TEXT("SendJsonResponse: Payload received (truncated): %s"), *JsonPayload.Left(1000));
	}
	else
	{
		UE_LOG(LogUnrealMCPServer, Verbose, TEXT("SendJsonResponse: Payload received: %s"), *JsonPayload);
	}
    TUniquePtr<FHttpServerResponse> Response = FHttpServerResponse::Create(JsonPayload, TEXT("application/json"));
    Response->Code = EHttpServerResponseCodes::Ok; 

    if (!Response.IsValid())
    {
        UE_LOG(LogUnrealMCPServer, Error, TEXT("SendJsonResponse: FHttpServerResponse::Create failed to create a valid response object!"));
        return; 
    }
    
    UE_LOG(LogUnrealMCPServer, Verbose, TEXT("SendJsonResponse: Calling OnComplete. Response Code: %d"), Response->Code);
    OnComplete(MoveTemp(Response));
}

// Main handler for MCP requests, runs on an HTTP thread
void FUMCP_Server::HandleStreamableHTTPMCPRequest(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
	FUTF8ToTCHAR Convert((ANSICHAR*)Request.Body.GetData(), Request.Body.Num());
	FString RequestBody(Convert.Length(), Convert.Get());
    UE_LOG(LogUnrealMCPServer, Verbose, TEXT("Received MCP request: %s"), *RequestBody);
	
	FUMCP_JsonRpcResponse Response;

    FUMCP_JsonRpcRequest RpcRequest;
    if (!FUMCP_JsonRpcRequest::CreateFromJsonString(RequestBody, RpcRequest))
    {
        UE_LOG(LogUnrealMCPServer, Error, TEXT("Failed to parse MCP request JSON: %s"), *RequestBody);
    	Response.error = MakeShared<FUMCP_JsonRpcError>(EUMCP_JsonRpcErrorCode::ParseError, TEXT("Failed to parse MCP request JSON"));
        SendJsonRpcResponse(OnComplete, Response);
        return;
    }
	Response.id = RpcRequest.id;

    if (RpcRequest.jsonrpc != TEXT("2.0"))
    {
        UE_LOG(LogUnrealMCPServer, Error, TEXT("Invalid JSON-RPC version: %s"), *RpcRequest.jsonrpc);
		Response.error = MakeShared<FUMCP_JsonRpcError>(EUMCP_JsonRpcErrorCode::InvalidRequest, TEXT("Invalid Request - JSON-RPC version must be 2.0"));
        SendJsonRpcResponse(OnComplete, Response);
        return;
    }

	UMCP_JsonRpcHandler* Handler = JsonRpcMethodHandlers.Find(RpcRequest.method);
	if (!Handler)
	{
        UE_LOG(LogUnrealMCPServer, Warning, TEXT("Unknown MCP method received: %s"), *RpcRequest.method);
		Response.error = MakeShared<FUMCP_JsonRpcError>(EUMCP_JsonRpcErrorCode::MethodNotFound, TEXT("Method not found"));
		SendJsonRpcResponse(OnComplete, Response);
		return;
	}

	auto SuccessObject = MakeShared<FJsonObject>();
	auto ErrorObject = MakeShared<FUMCP_JsonRpcError>();
	if (!(*Handler)(RpcRequest, SuccessObject, *ErrorObject))
	{
		UE_LOG(LogUnrealMCPServer, Warning, TEXT("Error handling '%s': (%d) %s"), *RpcRequest.method, ErrorObject->code, *ErrorObject->message);
		Response.error = MoveTemp(ErrorObject);
		SendJsonRpcResponse(OnComplete, Response);
		return;
	}
	Response.result = MakeShared<FJsonValueObject>(MoveTemp(SuccessObject));
	SendJsonRpcResponse(OnComplete, Response);
}

void FUMCP_Server::RegisterInternalRpcMethodHandlers()
{
	// General 
	RegisterRpcMethodHandler(TEXT("initialize"), [this](const FUMCP_JsonRpcRequest& Request, TSharedPtr<FJsonObject> OutSuccess, FUMCP_JsonRpcError& OutError)
	{
		return Rpc_Initialize(Request, OutSuccess, OutError);
	});
	RegisterRpcMethodHandler(TEXT("ping"), [this](const FUMCP_JsonRpcRequest& Request, TSharedPtr<FJsonObject> OutSuccess, FUMCP_JsonRpcError& OutError)
	{
		return Rpc_Ping(Request, OutSuccess, OutError);
	});
	RegisterRpcMethodHandler(TEXT("notifications/initialized"), [this](const FUMCP_JsonRpcRequest& Request, TSharedPtr<FJsonObject> OutSuccess, FUMCP_JsonRpcError& OutError)
	{
		return Rpc_ClientNotifyInitialized(Request, OutSuccess, OutError);
	});

	// Tools
	RegisterRpcMethodHandler(TEXT("tools/list"), [this](const FUMCP_JsonRpcRequest& Request, TSharedPtr<FJsonObject> OutSuccess, FUMCP_JsonRpcError& OutError)
	{
		return Rpc_ToolsList(Request, OutSuccess, OutError);
	});
	RegisterRpcMethodHandler(TEXT("tools/call"), [this](const FUMCP_JsonRpcRequest& Request, TSharedPtr<FJsonObject> OutSuccess, FUMCP_JsonRpcError& OutError)
	{
		return Rpc_ToolsCall(Request, OutSuccess, OutError);
	});

	// Resources
	RegisterRpcMethodHandler(TEXT("resources/list"), [this](const FUMCP_JsonRpcRequest& Request, TSharedPtr<FJsonObject> OutSuccess, FUMCP_JsonRpcError& OutError)
	{
		return Rpc_ResourcesList(Request, OutSuccess, OutError);
	});
	RegisterRpcMethodHandler(TEXT("resources/templates/list"), [this](const FUMCP_JsonRpcRequest& Request, TSharedPtr<FJsonObject> OutSuccess, FUMCP_JsonRpcError& OutError)
	{
		return Rpc_ResourcesTemplatesList(Request, OutSuccess, OutError);
	});
	RegisterRpcMethodHandler(TEXT("resources/read"), [this](const FUMCP_JsonRpcRequest& Request, TSharedPtr<FJsonObject> OutSuccess, FUMCP_JsonRpcError& OutError)
	{
		return Rpc_ResourcesRead(Request, OutSuccess, OutError);
	});

	// Prompts
	RegisterRpcMethodHandler(TEXT("prompts/list"), [this](const FUMCP_JsonRpcRequest& Request, TSharedPtr<FJsonObject> OutSuccess, FUMCP_JsonRpcError& OutError)
	{
		return Rpc_PromptsList(Request, OutSuccess, OutError);
	});
	RegisterRpcMethodHandler(TEXT("prompts/get"), [this](const FUMCP_JsonRpcRequest& Request, TSharedPtr<FJsonObject> OutSuccess, FUMCP_JsonRpcError& OutError)
	{
		return Rpc_PromptsGet(Request, OutSuccess, OutError);
	});
}

bool FUMCP_Server::Rpc_Initialize(const FUMCP_JsonRpcRequest& Request, TSharedPtr<FJsonObject> OutSuccess, FUMCP_JsonRpcError& OutError)
{
	FUMCP_InitializeParams Params;
	if (!UMCP_CreateFromJsonObject(Request.params, Params))
	{
		OutError.SetError(EUMCP_JsonRpcErrorCode::InvalidParams);
		OutError.message = TEXT("Failed to parse 'initialize' params");
        return false;
	}

	// TODO proper capabilities negotiation
	
	FUMCP_InitializeResult Result;
	Result.protocolVersion = MCP_PROTOCOL_VERSION; // Server's supported version
	Result.serverInfo.name = TEXT("UnrealMCPServer");
	Result.serverInfo.version = PLUGIN_VERSION + TEXT(" (") + FEngineVersion::Current().ToString(EVersionComponent::Patch) + TEXT(")");

	// Populate ServerCapabilities (defaults are fine for now as defined in MCPTypes.h constructors)
	// FServerCapabilities members are default-initialized. Example: Result.serverCapabilities.tools.inputSchema = true;

	if (!UMCP_ToJsonObject(Result, OutSuccess))
	{
		OutError.SetError(EUMCP_JsonRpcErrorCode::InternalError);
		OutError.message = TEXT("Failed to serialize initialize result");
		return false;
	}

	return true;
}

bool FUMCP_Server::Rpc_Ping(const FUMCP_JsonRpcRequest& Request, TSharedPtr<FJsonObject> OutSuccess, FUMCP_JsonRpcError& OutError)
{
	UE_LOG(LogUnrealMCPServer, Verbose, TEXT("Handling ping method."));
	return true;
}

bool FUMCP_Server::Rpc_ClientNotifyInitialized(const FUMCP_JsonRpcRequest& Request, TSharedPtr<FJsonObject> OutSuccess, FUMCP_JsonRpcError& OutError)
{
	UE_LOG(LogUnrealMCPServer, Verbose, TEXT("Handling ClientNotifyInitialized method."));
	return true;
}

bool FUMCP_Server::Rpc_ToolsList(const FUMCP_JsonRpcRequest& Request, TSharedPtr<FJsonObject> OutSuccess, FUMCP_JsonRpcError& OutError)
{
	FUMCP_ListToolsParams Params;
	if (!UMCP_CreateFromJsonObject(Request.params, Params, true))
	{
		OutError.SetError(EUMCP_JsonRpcErrorCode::InvalidParams);
		OutError.message = TEXT("Failed to parse list tools params");
        return false;
	}

	// Unfortunately, unreal doesn't have a good json serialization override and the json tools have
	// a schema that was too complicated to deal with the complexity in the middle.
	// So for now we manually add them to the response array here.
	TArray<TSharedPtr<FJsonValue>> ResultTools;
	for (auto Itr = Tools.CreateConstIterator(); Itr; ++Itr)
	{
		auto ToolDef = MakeShared<FJsonObject>();
		ToolDef->SetStringField(TEXT("name"), Itr->Key);
		ToolDef->SetStringField(TEXT("description"), Itr->Value.description);
		ToolDef->SetObjectField(TEXT("inputSchema"), Itr->Value.inputSchema);
		if (Itr->Value.outputSchema.IsValid())
		{
			ToolDef->SetObjectField(TEXT("outputSchema"), Itr->Value.outputSchema);
		}
		ResultTools.Add(MakeShared<FJsonValueObject>(MoveTemp(ToolDef)));
	}
	OutSuccess->SetArrayField(TEXT("tools"), MoveTemp(ResultTools));
	// `nextCursor` is blank, since we return them all
	return true;
}

bool FUMCP_Server::Rpc_ToolsCall(const FUMCP_JsonRpcRequest& Request, TSharedPtr<FJsonObject> OutSuccess, FUMCP_JsonRpcError& OutError)
{
	FUMCP_CallToolParams Params;
	UMCP_CreateFromJsonObject(Request.params, Params);
	auto* Tool = Tools.Find(Params.name);
	if (!Tool)
	{
		OutError.SetError(EUMCP_JsonRpcErrorCode::InvalidParams);
		OutError.message = TEXT("Unknown tool name");
		return false;
	}

	if (!Tool->DoToolCall.IsBound())
	{
		OutError.SetError(EUMCP_JsonRpcErrorCode::InternalError);
		OutError.message = TEXT("Tool has no bound delegate");
		return false;
	}

	FUMCP_CallToolResult Result;
	Result.isError = !Tool->DoToolCall.Execute(Params.arguments, Result.content);
	
	// Flush the log file after tool execution to ensure all log entries are written before returning the response
	if (GLog)
	{
		GLog->Flush();
	}
	
	if (!UMCP_ToJsonObject(Result, OutSuccess))
	{
		OutError.SetError(EUMCP_JsonRpcErrorCode::InternalError);
		OutError.message = TEXT("Failed to serialize result");
		return false;
	}
	
	// If tool has an outputSchema and the result is not an error, extract structured content
	if (Tool->outputSchema.IsValid() && !Result.isError && Result.content.Num() > 0)
	{
		// Try to parse the first text content as JSON to extract structured content
		const FUMCP_CallToolResultContent& FirstContent = Result.content[0];
		if (FirstContent.type == TEXT("text") && !FirstContent.text.IsEmpty())
		{
			TSharedPtr<FJsonObject> StructuredContent;
			TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(FirstContent.text);
			if (FJsonSerializer::Deserialize(Reader, StructuredContent) && StructuredContent.IsValid())
			{
				// Validate that the structured content has the required properties from the outputSchema
				const TSharedPtr<FJsonObject>& Schema = Tool->outputSchema;
				if (Schema.IsValid())
				{
					// Check if schema has a "required" array
					const TArray<TSharedPtr<FJsonValue>>* RequiredArray = nullptr;
					if (Schema->TryGetArrayField(TEXT("required"), RequiredArray) && RequiredArray)
					{
						bool bAllRequiredPresent = true;
						for (const TSharedPtr<FJsonValue>& RequiredValue : *RequiredArray)
						{
							FString RequiredField;
							if (RequiredValue->TryGetString(RequiredField))
							{
								if (!StructuredContent->HasField(RequiredField))
								{
									UE_LOG(LogUnrealMCPServer, Warning, TEXT("Tool '%s' structuredContent missing required field '%s'"), *Params.name, *RequiredField);
									bAllRequiredPresent = false;
								}
							}
						}
						
						if (!bAllRequiredPresent)
						{
							// Log the actual structure for debugging
							FString StructuredContentStr;
							TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&StructuredContentStr);
							FJsonSerializer::Serialize(StructuredContent.ToSharedRef(), Writer);
							UE_LOG(LogUnrealMCPServer, Warning, TEXT("Tool '%s' structuredContent structure: %s"), *Params.name, *StructuredContentStr);
						}
					}
				}
				
				// Log the structured content for debugging (truncated if too long)
				FString StructuredContentStr;
				TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&StructuredContentStr);
				FJsonSerializer::Serialize(StructuredContent.ToSharedRef(), Writer);
				if (StructuredContentStr.Len() > 500)
				{
					UE_LOG(LogUnrealMCPServer, Verbose, TEXT("Tool '%s' structuredContent (truncated): %s"), *Params.name, *StructuredContentStr.Left(500));
				}
				else
				{
					UE_LOG(LogUnrealMCPServer, Verbose, TEXT("Tool '%s' structuredContent: %s"), *Params.name, *StructuredContentStr);
				}
				
				// Add structuredContent field to the result
				OutSuccess->SetObjectField(TEXT("structuredContent"), StructuredContent);
			}
			else
			{
				UE_LOG(LogUnrealMCPServer, Warning, TEXT("Failed to parse structured content from tool '%s' result text. Text length: %d"), *Params.name, FirstContent.text.Len());
				if (FirstContent.text.Len() < 500)
				{
					UE_LOG(LogUnrealMCPServer, Warning, TEXT("Failed to parse text content: %s"), *FirstContent.text);
				}
			}
		}
	}
	
	return true;
}

bool FUMCP_Server::Rpc_ResourcesList(const FUMCP_JsonRpcRequest& Request, TSharedPtr<FJsonObject> OutSuccess, FUMCP_JsonRpcError& OutError)
{
	FUMCP_ListResourcesParams Params;
	if (!UMCP_CreateFromJsonObject(Request.params, Params, true))
	{
		OutError.SetError(EUMCP_JsonRpcErrorCode::InvalidParams);
		OutError.message = TEXT("Failed to parse list resources params");
        return false;
	}

	FUMCP_ListResourcesResult Result;
	for (auto Itr = Resources.CreateConstIterator(); Itr; ++Itr)
	{
		Result.resources.Add(Itr->Value);
	}
	
	if (!UMCP_ToJsonObject(Result, OutSuccess))
	{
		OutError.SetError(EUMCP_JsonRpcErrorCode::InternalError);
		OutError.message = TEXT("Failed to serialize result");
		return false;
	}
	return true;
}

bool FUMCP_Server::Rpc_ResourcesTemplatesList(const FUMCP_JsonRpcRequest& Request, TSharedPtr<FJsonObject> OutSuccess, FUMCP_JsonRpcError& OutError)
{
	FUMCP_ListResourceTemplatesParams Params;
	if (!UMCP_CreateFromJsonObject(Request.params, Params, true))
	{
		OutError.SetError(EUMCP_JsonRpcErrorCode::InvalidParams);
		OutError.message = TEXT("Failed to parse list resources params");
        return false;
	}

	FUMCP_ListResourceTemplatesResult Result;
	for (auto Itr = ResourceTemplates.CreateConstIterator(); Itr; Itr++)
	{
		Result.resourceTemplates.Emplace(Itr->Value);
	}
	if (!UMCP_ToJsonObject(Result, OutSuccess))
	{
		OutError.SetError(EUMCP_JsonRpcErrorCode::InternalError);
		OutError.message = TEXT("Failed to serialize result");
		return false;
	}
	return true;
}

bool FUMCP_Server::Rpc_ResourcesRead(const FUMCP_JsonRpcRequest& Request, TSharedPtr<FJsonObject> OutSuccess, FUMCP_JsonRpcError& OutError)
{
	FUMCP_ReadResourceParams Params;
	if (!UMCP_CreateFromJsonObject(Request.params, Params))
	{
		OutError.SetError(EUMCP_JsonRpcErrorCode::InvalidParams);
		OutError.message = TEXT("Failed to parse read resource params");
        return false;
	}

	FUMCP_ReadResourceResult Result;

	// First check our static resources (since the check is easier)
	auto* Resource = Resources.Find(Params.uri);
	if (Resource && Resource->ReadResource.IsBound())
	{
		if (!Resource->ReadResource.Execute(Params.uri, Result.contents))
		{
			OutError.SetError(EUMCP_JsonRpcErrorCode::ResourceNotFound);
			OutError.message = TEXT("Failed to load resource contents");
			return false;
		}
		
		if (!UMCP_ToJsonObject(Result, OutSuccess))
		{
			OutError.SetError(EUMCP_JsonRpcErrorCode::InternalError);
			OutError.message = TEXT("Failed to serialize result");
			return false;
		}
		return true;
	}

	// Check resouce templates
	for (auto Itr = ResourceTemplates.CreateConstIterator(); Itr; ++Itr)
	{
		auto& ResourceTemplate = Itr->Value;
		if (!ResourceTemplate.ReadResource.IsBound())
		{
			continue;
		}
		
		auto& UriTemplate = Itr->Key;
		FUMCP_UriTemplateMatch Match;
		if (!UriTemplate.FindMatch(Params.uri, Match))
		{
			continue;
		}

		if (!ResourceTemplate.ReadResource.Execute(UriTemplate, Match, Result.contents))
		{
			OutError.SetError(EUMCP_JsonRpcErrorCode::InternalError);
			OutError.message = TEXT("Failed to load resource contents");
			return false;
		}
		
		if (!UMCP_ToJsonObject(Result, OutSuccess))
		{
			OutError.SetError(EUMCP_JsonRpcErrorCode::InternalError);
			OutError.message = TEXT("Failed to serialize result");
			return false;
		}
		return true;
	}
	
	OutError.SetError(EUMCP_JsonRpcErrorCode::ResourceNotFound);
	OutError.message = TEXT("Resource not found");
	return false;
}

bool FUMCP_Server::Rpc_PromptsList(const FUMCP_JsonRpcRequest& Request, TSharedPtr<FJsonObject> OutSuccess, FUMCP_JsonRpcError& OutError)
{
	FUMCP_ListPromptsParams Params;
	if (!UMCP_CreateFromJsonObject(Request.params, Params, true))
	{
		OutError.SetError(EUMCP_JsonRpcErrorCode::InvalidParams);
		OutError.message = TEXT("Failed to parse list prompts params");
		return false;
	}

	// Build prompts array manually (similar to tools)
	TArray<TSharedPtr<FJsonValue>> ResultPrompts;
	for (auto Itr = Prompts.CreateConstIterator(); Itr; ++Itr)
	{
		const FUMCP_PromptDefinitionInternal& PromptDef = Itr->Value;
		auto PromptJson = MakeShared<FJsonObject>();
		PromptJson->SetStringField(TEXT("name"), PromptDef.name);
		if (!PromptDef.title.IsEmpty())
		{
			PromptJson->SetStringField(TEXT("title"), PromptDef.title);
		}
		if (!PromptDef.description.IsEmpty())
		{
			PromptJson->SetStringField(TEXT("description"), PromptDef.description);
		}
		
		// Add arguments array if present
		if (PromptDef.arguments.Num() > 0)
		{
			TArray<TSharedPtr<FJsonValue>> ArgumentsArray;
			for (const FUMCP_PromptArgument& Arg : PromptDef.arguments)
			{
				auto ArgJson = MakeShared<FJsonObject>();
				ArgJson->SetStringField(TEXT("name"), Arg.name);
				if (!Arg.description.IsEmpty())
				{
					ArgJson->SetStringField(TEXT("description"), Arg.description);
				}
				ArgJson->SetBoolField(TEXT("required"), Arg.required);
				ArgumentsArray.Add(MakeShared<FJsonValueObject>(ArgJson));
			}
			PromptJson->SetArrayField(TEXT("arguments"), ArgumentsArray);
		}
		
		ResultPrompts.Add(MakeShared<FJsonValueObject>(MoveTemp(PromptJson)));
	}
	
	OutSuccess->SetArrayField(TEXT("prompts"), MoveTemp(ResultPrompts));
	// `nextCursor` is blank, since we return them all
	OutSuccess->SetStringField(TEXT("nextCursor"), TEXT(""));
	
	return true;
}

bool FUMCP_Server::Rpc_PromptsGet(const FUMCP_JsonRpcRequest& Request, TSharedPtr<FJsonObject> OutSuccess, FUMCP_JsonRpcError& OutError)
{
	FUMCP_GetPromptParams Params;
	if (!UMCP_CreateFromJsonObject(Request.params, Params))
	{
		OutError.SetError(EUMCP_JsonRpcErrorCode::InvalidParams);
		OutError.message = TEXT("Failed to parse get prompt params");
		return false;
	}

	if (Params.name.IsEmpty())
	{
		OutError.SetError(EUMCP_JsonRpcErrorCode::InvalidParams);
		OutError.message = TEXT("Missing required parameter: name");
		return false;
	}

	auto* Prompt = Prompts.Find(Params.name);
	if (!Prompt)
	{
		OutError.SetError(EUMCP_JsonRpcErrorCode::InvalidParams);
		OutError.message = FString::Printf(TEXT("Prompt not found: %s"), *Params.name);
		return false;
	}

	if (!Prompt->GetPrompt.IsBound())
	{
		OutError.SetError(EUMCP_JsonRpcErrorCode::InternalError);
		OutError.message = TEXT("Prompt has no bound delegate");
		return false;
	}

	// Call the prompt delegate to get messages
	TArray<FUMCP_PromptMessage> Messages = Prompt->GetPrompt.Execute(Params.arguments);

	// Build result JSON
	if (!Prompt->description.IsEmpty())
	{
		OutSuccess->SetStringField(TEXT("description"), Prompt->description);
	}

	// Convert messages to JSON array
	TArray<TSharedPtr<FJsonValue>> MessagesArray;
	for (const FUMCP_PromptMessage& Message : Messages)
	{
		auto MessageJson = MakeShared<FJsonObject>();
		MessageJson->SetStringField(TEXT("role"), Message.role);
		
		// Add content object if present
		if (Message.content.IsValid())
		{
			MessageJson->SetObjectField(TEXT("content"), Message.content);
		}
		
		MessagesArray.Add(MakeShared<FJsonValueObject>(MoveTemp(MessageJson)));
	}
	
	OutSuccess->SetArrayField(TEXT("messages"), MoveTemp(MessagesArray));
	
	return true;
}
