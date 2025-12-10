#include "UMCP_CommonTools.h"
#include "UMCP_Server.h"
#include "UMCP_Types.h"
#include "UnrealMCPServerModule.h"
#include "Misc/Paths.h"
#include "Misc/EngineVersion.h"
#include "Engine/Engine.h"
#include "HAL/IConsoleManager.h"
#include "Toolkits/FConsoleCommandExecutor.h"
#include "Editor/UnrealEd/Public/Editor.h"
#include "HAL/PlatformOutputDevices.h"

void FUMCP_CommonTools::Register(class FUMCP_Server* Server)
{
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
	
	{
		FUMCP_ToolDefinition Tool;
		Tool.name = TEXT("execute_console_command");
		Tool.description = TEXT("Execute an Unreal Engine console command and return its output. This allows running any console command available in the Unreal Engine editor.");
		Tool.DoToolCall.BindRaw(this, &FUMCP_CommonTools::ExecuteConsoleCommand);
		
		// Generate input schema from USTRUCT with descriptions
		TMap<FString, FString> InputDescriptions;
		InputDescriptions.Add(TEXT("command"), TEXT("The console command to execute (e.g., 'stat fps', 'showdebug ai', 'r.SetRes 1920x1080')"));
		TArray<FString> InputRequired;
		InputRequired.Add(TEXT("command"));
		Tool.inputSchema = UMCP_GenerateJsonSchemaFromStruct(FUMCP_ExecuteConsoleCommandParams::StaticStruct(), InputDescriptions, InputRequired);
		
		// Generate output schema from USTRUCT with descriptions
		TMap<FString, FString> OutputDescriptions;
		OutputDescriptions.Add(TEXT("bSuccess"), TEXT("Whether the command was executed successfully"));
		OutputDescriptions.Add(TEXT("command"), TEXT("The command that was executed"));
		OutputDescriptions.Add(TEXT("output"), TEXT("The output from the console command (if any)"));
		OutputDescriptions.Add(TEXT("error"), TEXT("Error message if bSuccess is false"));
		TArray<FString> OutputRequired;
		OutputRequired.Add(TEXT("bSuccess"));
		OutputRequired.Add(TEXT("command"));
		TSharedPtr<FJsonObject> ExecuteConsoleCommandOutputSchema = UMCP_GenerateJsonSchemaFromStruct(FUMCP_ExecuteConsoleCommandResult::StaticStruct(), OutputDescriptions, OutputRequired);
		if (ExecuteConsoleCommandOutputSchema.IsValid())
		{
			Tool.outputSchema = ExecuteConsoleCommandOutputSchema;
		}
		else
		{
			UE_LOG(LogUnrealMCPServer, Error, TEXT("Failed to generate outputSchema for execute_console_command tool"));
		}
		Server->RegisterTool(MoveTemp(Tool));
	}
	
	{
		FUMCP_ToolDefinition Tool;
		Tool.name = TEXT("get_log_file_path");
		Tool.description = TEXT("Returns the path of the Unreal Engine log file.");
		Tool.DoToolCall.BindRaw(this, &FUMCP_CommonTools::GetLogFilePath);
		
		// Generate input schema from USTRUCT (empty params struct)
		Tool.inputSchema = UMCP_GenerateJsonSchemaFromStruct(FUMCP_GetLogFilePathParams::StaticStruct());
		
		// Generate output schema from USTRUCT with descriptions
		TMap<FString, FString> OutputDescriptions;
		OutputDescriptions.Add(TEXT("logFilePath"), TEXT("The full path to the Unreal Engine log file"));
		TArray<FString> OutputRequired;
		OutputRequired.Add(TEXT("logFilePath"));
		TSharedPtr<FJsonObject> GetLogFilePathOutputSchema = UMCP_GenerateJsonSchemaFromStruct(FUMCP_GetLogFilePathResult::StaticStruct(), OutputDescriptions, OutputRequired);
		if (GetLogFilePathOutputSchema.IsValid())
		{
			Tool.outputSchema = GetLogFilePathOutputSchema;
		}
		else
		{
			UE_LOG(LogUnrealMCPServer, Error, TEXT("Failed to generate outputSchema for get_log_file_path tool"));
		}
		Server->RegisterTool(MoveTemp(Tool));
	}
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

bool FUMCP_CommonTools::ExecuteConsoleCommand(TSharedPtr<FJsonObject> arguments, TArray<FUMCP_CallToolResultContent>& OutContent)
{
	auto& Content = OutContent.Add_GetRef(FUMCP_CallToolResultContent());
	Content.type = TEXT("text");

	// Convert JSON to USTRUCT at the start
	FUMCP_ExecuteConsoleCommandParams Params;
	if (!arguments.IsValid() || !UMCP_CreateFromJsonObject(arguments, Params))
	{
		FUMCP_ExecuteConsoleCommandResult ErrorResult;
		ErrorResult.bSuccess = false;
		ErrorResult.command = TEXT("");
		ErrorResult.output = TEXT("");
		ErrorResult.error = TEXT("Invalid parameters");
		if (!UMCP_ToJsonString(ErrorResult, Content.text))
		{
			Content.text = TEXT("Failed to serialize error result");
		}
		return false;
	}

	// Work with USTRUCT throughout
	FUMCP_ExecuteConsoleCommandResult Result;
	Result.bSuccess = false;
	Result.command = Params.command;

	if (Params.command.IsEmpty())
	{
		Result.output = TEXT("");
		Result.error = TEXT("Missing required parameter: command");
		if (!UMCP_ToJsonString(Result, Content.text))
		{
			Content.text = TEXT("Failed to serialize error result");
		}
		return false;
	}

	UE_LOG(LogUnrealMCPServer, Log, TEXT("ExecuteConsoleCommand: Executing command '%s'"), *Params.command);

	// Get the world context (prefer PIE world, then editor world)
	UWorld* World = nullptr;
	if (GEditor)
	{
		// Try to get PIE world first
		if (GEditor->PlayWorld)
		{
			World = GEditor->PlayWorld;
		}
		else
		{
			// Fall back to editor world
			World = GEditor->GetEditorWorldContext().World();
		}
	}

	// Use FConsoleCommandExecutor to execute the command
	FConsoleCommandExecutor Executor;
	
	// Execute the command using FConsoleCommandExecutor
	bool bCommandExecuted = Executor.Exec(*Params.command);
	
	// Note: FConsoleCommandExecutor routes output to the default output device
	// The actual output will be visible in the Unreal Engine output log
	
	if (bCommandExecuted)
	{
		Result.bSuccess = true;
		Result.output = TEXT("Command executed successfully. Check the Unreal Engine output log for command output.");
		Result.error = TEXT("");
	}
	else
	{
		Result.bSuccess = false;
		Result.output = TEXT("");
		Result.error = FString::Printf(TEXT("Command execution failed or command not recognized: %s"), *Params.command);
	}

	// Convert USTRUCT to JSON string at the end
	if (!UMCP_ToJsonString(Result, Content.text))
	{
		Content.text = TEXT("Failed to serialize result");
		return false;
	}

	UE_LOG(LogUnrealMCPServer, Log, TEXT("ExecuteConsoleCommand: Command '%s' executed, success=%s"), *Params.command, Result.bSuccess ? TEXT("true") : TEXT("false"));
	
	return true;
}

bool FUMCP_CommonTools::GetLogFilePath(TSharedPtr<FJsonObject> arguments, TArray<FUMCP_CallToolResultContent>& OutContent)
{
	auto& Content = OutContent.Add_GetRef(FUMCP_CallToolResultContent());
	Content.type = TEXT("text");

	// Convert JSON to USTRUCT (no params needed, but validate structure)
	FUMCP_GetLogFilePathParams Params;
	if (arguments.IsValid() && !UMCP_CreateFromJsonObject(arguments, Params, true))
	{
		Content.text = TEXT("Invalid parameters");
		return false;
	}

	// Work with USTRUCT throughout
	FUMCP_GetLogFilePathResult Result;
	
	// Get the log file path from the platform output devices
	FString LogFilePath = FPlatformOutputDevices::GetAbsoluteLogFilename();
	
	// Ensure the path is absolute (convert relative to absolute if needed)
	// Check if path is relative by seeing if it doesn't start with a drive letter (Windows) or "/" (Unix)
	bool bIsRelative = !LogFilePath.StartsWith(TEXT("/")) && 
	                   (LogFilePath.Len() < 2 || LogFilePath[1] != TEXT(':'));
	
	if (bIsRelative)
	{
		// If relative, combine with project directory first, then convert to absolute
		FString CombinedPath = FPaths::Combine(FPaths::ProjectDir(), LogFilePath);
		LogFilePath = FPaths::ConvertRelativePathToFull(CombinedPath);
	}
	else
	{
		// Path appears absolute, but ensure it's fully normalized
		LogFilePath = FPaths::ConvertRelativePathToFull(LogFilePath);
	}
	
	Result.logFilePath = LogFilePath;

	// Convert USTRUCT to JSON string at the end
	if (!UMCP_ToJsonString(Result, Content.text))
	{
		Content.text = TEXT("Failed to serialize result");
		return false;
	}
	
	UE_LOG(LogUnrealMCPServer, Log, TEXT("GetLogFilePath: Retrieved log file path: %s"), *Result.logFilePath);
	
	return true;
}
