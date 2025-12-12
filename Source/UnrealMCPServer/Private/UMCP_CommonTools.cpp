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
#include "ILiveCodingModule.h"
#include "Modules/ModuleManager.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "Misc/FileHelper.h"

void FUMCP_CommonTools::Register(class FUMCP_Server* Server)
{
	{
		FUMCP_ToolDefinition Tool;
		Tool.name = TEXT("get_project_config");
		Tool.description = TEXT("Retrieve project and engine configuration information including engine version, directory paths (Engine, Project, Content, Log, Saved, Config, Plugins), and other essential project metadata. Use this tool first to understand the project structure before performing asset operations. Returns absolute paths that can be used in other tool calls.");
		Tool.DoToolCall.BindRaw(this, &FUMCP_CommonTools::GetProjectConfig);
		
		// Generate input schema from USTRUCT (empty params struct)
		Tool.inputSchema = UMCP_GenerateJsonSchemaFromStruct<FUMCP_GetProjectConfigParams>();
		
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
		
		TSharedPtr<FJsonObject> ProjectConfigOutputSchema = UMCP_GenerateJsonSchemaFromStruct<FUMCP_GetProjectConfigResult>(OutputDescriptions, OutputRequired);
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
		Tool.description = TEXT("Execute an Unreal Engine console command and return its output. This allows running any console command available in the Unreal Engine editor. Common commands: 'stat fps' (performance stats), 'showdebug ai' (AI debugging), 'r.SetRes 1920x1080' (set resolution), 'open /Game/Maps/MainLevel' (load level), 'stat unit' (frame timing). Note: Some commands modify editor state. Returns command output as a string. Some commands may return empty strings if they only produce visual output in the editor.");
		Tool.DoToolCall.BindRaw(this, &FUMCP_CommonTools::ExecuteConsoleCommand);
		
		// Generate input schema from USTRUCT with descriptions
		TMap<FString, FString> InputDescriptions;
		InputDescriptions.Add(TEXT("command"), TEXT("The console command to execute. Examples: 'stat fps' (performance), 'showdebug ai' (AI debugging), 'r.SetRes 1920x1080' (resolution), 'open /Game/Maps/MainLevel' (load level), 'stat unit' (frame timing), 'quit' (exit editor). Warning: Some commands can modify the editor state or project. Use with caution for commands that modify assets or project settings."));
		TArray<FString> InputRequired;
		InputRequired.Add(TEXT("command"));
		Tool.inputSchema = UMCP_GenerateJsonSchemaFromStruct<FUMCP_ExecuteConsoleCommandParams>(InputDescriptions, InputRequired);
		
		// Generate output schema from USTRUCT with descriptions
		TMap<FString, FString> OutputDescriptions;
		OutputDescriptions.Add(TEXT("bSuccess"), TEXT("Whether the command was executed successfully"));
		OutputDescriptions.Add(TEXT("command"), TEXT("The command that was executed"));
		OutputDescriptions.Add(TEXT("output"), TEXT("The output from the console command (if any)"));
		OutputDescriptions.Add(TEXT("error"), TEXT("Error message if bSuccess is false"));
		TArray<FString> OutputRequired;
		OutputRequired.Add(TEXT("bSuccess"));
		OutputRequired.Add(TEXT("command"));
		TSharedPtr<FJsonObject> ExecuteConsoleCommandOutputSchema = UMCP_GenerateJsonSchemaFromStruct<FUMCP_ExecuteConsoleCommandResult>(OutputDescriptions, OutputRequired);
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
		Tool.description = TEXT("Returns the absolute path of the Unreal Engine log file. Use this to locate log files for debugging. Log files are plain text and can be read with standard file reading tools. Note: The log file path changes when the editor restarts. Call this tool when you need the current log file location.");
		Tool.DoToolCall.BindRaw(this, &FUMCP_CommonTools::GetLogFilePath);
		
		// Generate input schema from USTRUCT (empty params struct)
		Tool.inputSchema = UMCP_GenerateJsonSchemaFromStruct<FUMCP_GetLogFilePathParams>();
		
		// Generate output schema from USTRUCT with descriptions
		TMap<FString, FString> OutputDescriptions;
		OutputDescriptions.Add(TEXT("logFilePath"), TEXT("The full path to the Unreal Engine log file"));
		TArray<FString> OutputRequired;
		OutputRequired.Add(TEXT("logFilePath"));
		TSharedPtr<FJsonObject> GetLogFilePathOutputSchema = UMCP_GenerateJsonSchemaFromStruct<FUMCP_GetLogFilePathResult>(OutputDescriptions, OutputRequired);
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
	
	{
		FUMCP_ToolDefinition Tool;
		Tool.name = TEXT("request_editor_compile");
		Tool.description = TEXT("Requests an editor compilation, waits for completion, and returns whether it succeeded or failed along with any build log generated. Use this after modifying C++ source files to recompile code changes without restarting the editor. Only works if the project has C++ code and live coding is enabled in editor settings. Default timeout is 300 seconds (5 minutes). Compilation may take longer for large projects. Returns success status, build log, and extracted errors/warnings. Check the build log for compilation errors if compilation fails.");
		Tool.DoToolCall.BindRaw(this, &FUMCP_CommonTools::RequestEditorCompile);
		
		// Generate input schema from USTRUCT with descriptions
		TMap<FString, FString> InputDescriptions;
		InputDescriptions.Add(TEXT("timeoutSeconds"), TEXT("Optional timeout in seconds for waiting for compilation to complete. Default: 300 seconds (5 minutes). For large projects, you may need to increase this value. Compilation will be cancelled if it exceeds this timeout."));
		Tool.inputSchema = UMCP_GenerateJsonSchemaFromStruct<FUMCP_RequestEditorCompileParams>(InputDescriptions);
		
		// Generate output schema from USTRUCT with descriptions
		TMap<FString, FString> OutputDescriptions;
		OutputDescriptions.Add(TEXT("bSuccess"), TEXT("Whether the compilation completed successfully"));
		OutputDescriptions.Add(TEXT("bCompileStarted"), TEXT("Whether compilation was successfully initiated"));
		OutputDescriptions.Add(TEXT("status"), TEXT("Compilation status: 'completed', 'failed', 'timeout', 'not_available', or 'error'"));
		OutputDescriptions.Add(TEXT("buildLog"), TEXT("Full build log output from the compilation"));
		OutputDescriptions.Add(TEXT("errors"), TEXT("Array of extracted error messages from the build log"));
		OutputDescriptions.Add(TEXT("warnings"), TEXT("Array of extracted warning messages from the build log"));
		OutputDescriptions.Add(TEXT("error"), TEXT("Error message if bSuccess is false or status is 'error'"));
		TArray<FString> OutputRequired;
		OutputRequired.Add(TEXT("bSuccess"));
		OutputRequired.Add(TEXT("bCompileStarted"));
		OutputRequired.Add(TEXT("status"));
		TSharedPtr<FJsonObject> RequestEditorCompileOutputSchema = UMCP_GenerateJsonSchemaFromStruct<FUMCP_RequestEditorCompileResult>(OutputDescriptions, OutputRequired);
		if (RequestEditorCompileOutputSchema.IsValid())
		{
			Tool.outputSchema = RequestEditorCompileOutputSchema;
		}
		else
		{
			UE_LOG(LogUnrealMCPServer, Error, TEXT("Failed to generate outputSchema for request_editor_compile tool"));
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

bool FUMCP_CommonTools::RequestEditorCompile(TSharedPtr<FJsonObject> arguments, TArray<FUMCP_CallToolResultContent>& OutContent)
{
	auto& Content = OutContent.Add_GetRef(FUMCP_CallToolResultContent());
	Content.type = TEXT("text");

	// Convert JSON to USTRUCT at the start
	FUMCP_RequestEditorCompileParams Params;
	if (!arguments.IsValid() || !UMCP_CreateFromJsonObject(arguments, Params))
	{
		FUMCP_RequestEditorCompileResult ErrorResult;
		ErrorResult.bSuccess = false;
		ErrorResult.bCompileStarted = false;
		ErrorResult.status = TEXT("error");
		ErrorResult.buildLog = TEXT("");
		ErrorResult.error = TEXT("Invalid parameters");
		if (!UMCP_ToJsonString(ErrorResult, Content.text))
		{
			Content.text = TEXT("Failed to serialize error result");
		}
		return false;
	}

	// Work with USTRUCT throughout
	FUMCP_RequestEditorCompileResult Result;
	Result.bSuccess = false;
	Result.bCompileStarted = false;
	Result.status = TEXT("error");
	Result.buildLog = TEXT("");
	Result.error = TEXT("");

	UE_LOG(LogUnrealMCPServer, Log, TEXT("RequestEditorCompile: Requesting editor compilation (timeout: %.1f seconds)"), Params.timeoutSeconds);

	// Check if Live Coding is available
	ILiveCodingModule* LiveCodingModule = FModuleManager::GetModulePtr<ILiveCodingModule>(TEXT("LiveCoding"));
	if (!LiveCodingModule)
	{
		Result.status = TEXT("not_available");
		Result.error = TEXT("Live Coding module is not available. Ensure Live Coding is enabled in the editor settings.");
		if (!UMCP_ToJsonString(Result, Content.text))
		{
			Content.text = TEXT("Failed to serialize result");
		}
		return false;
	}

	// Check if Live Coding is enabled for this session
	if (!LiveCodingModule->IsEnabledForSession())
	{
		Result.status = TEXT("not_available");
		Result.error = TEXT("Live Coding is not enabled for this session. Enable Live Coding in the editor settings and restart the editor.");
		if (!UMCP_ToJsonString(Result, Content.text))
		{
			Content.text = TEXT("Failed to serialize result");
		}
		return false;
	}

	// Check if compilation is already in progress
	if (LiveCodingModule->IsCompiling())
	{
		Result.status = TEXT("error");
		Result.error = TEXT("A compilation is already in progress. Please wait for it to complete before requesting another compilation.");
		if (!UMCP_ToJsonString(Result, Content.text))
		{
			Content.text = TEXT("Failed to serialize result");
		}
		return false;
	}

	// Get the log file path to read build logs from
	FString LogFilePath = FPlatformOutputDevices::GetAbsoluteLogFilename();
	bool bIsRelative = !LogFilePath.StartsWith(TEXT("/")) && 
	                   (LogFilePath.Len() < 2 || LogFilePath[1] != TEXT(':'));
	
	if (bIsRelative)
	{
		FString CombinedPath = FPaths::Combine(FPaths::ProjectDir(), LogFilePath);
		LogFilePath = FPaths::ConvertRelativePathToFull(CombinedPath);
	}
	else
	{
		LogFilePath = FPaths::ConvertRelativePathToFull(LogFilePath);
	}

	// Request compilation using the Live Coding module API
	ELiveCodingCompileResult InitialCompileResult = ELiveCodingCompileResult::NotStarted;
	bool bCompileRequested = LiveCodingModule->Compile(ELiveCodingCompileFlags::None, &InitialCompileResult);
	
	// Check the initial result
	if (!bCompileRequested || InitialCompileResult == ELiveCodingCompileResult::NotStarted)
	{
		Result.bCompileStarted = false;
		Result.status = TEXT("error");
		Result.error = TEXT("Failed to start compilation. Live Coding may not be properly configured.");
		if (!UMCP_ToJsonString(Result, Content.text))
		{
			Content.text = TEXT("Failed to serialize result");
		}
		return false;
	}

	if (InitialCompileResult == ELiveCodingCompileResult::CompileStillActive)
	{
		Result.bCompileStarted = false;
		Result.status = TEXT("error");
		Result.error = TEXT("A compilation is already in progress.");
		if (!UMCP_ToJsonString(Result, Content.text))
		{
			Content.text = TEXT("Failed to serialize result");
		}
		return false;
	}

	// If we get here, compilation was started successfully
	// InitialCompileResult will be InProgress if compilation started, or Success/NoChanges/Failure if it completed immediately
	Result.bCompileStarted = true;
	
	// If compilation completed immediately, we can return early
	if (InitialCompileResult == ELiveCodingCompileResult::Success || 
	    InitialCompileResult == ELiveCodingCompileResult::NoChanges ||
	    InitialCompileResult == ELiveCodingCompileResult::Failure ||
	    InitialCompileResult == ELiveCodingCompileResult::Cancelled)
	{
		// Compilation completed immediately - get the UBT log and return
		ELiveCodingCompileResult FinalCompileResult = InitialCompileResult;
		return HandleCompilationComplete(LiveCodingModule, FinalCompileResult, Result, Content);
	}

	// Set up completion tracking using the delegate
	bool bCompilationComplete = false;
	
	// Bind to the patch complete delegate
	FDelegateHandle PatchCompleteHandle;
	PatchCompleteHandle = LiveCodingModule->GetOnPatchCompleteDelegate().AddLambda([&bCompilationComplete]()
	{
		bCompilationComplete = true;
	});

	// Wait for compilation to complete with timeout
	double StartTime = FPlatformTime::Seconds();
	double TimeoutTime = StartTime + Params.timeoutSeconds;
	bool bCompilationSucceeded = false;

	UE_LOG(LogUnrealMCPServer, Log, TEXT("RequestEditorCompile: Waiting for compilation to complete..."));

	while (!bCompilationComplete && FPlatformTime::Seconds() < TimeoutTime)
	{
		// Check if compilation is still in progress
		// The delegate will fire when complete, but we also check IsCompiling() as a fallback
		if (!LiveCodingModule->IsCompiling())
		{
			// If not compiling and we haven't received the delegate callback yet, 
			// give it a moment and then mark as complete
			FPlatformProcess::Sleep(0.2f);
			if (!LiveCodingModule->IsCompiling())
			{
				bCompilationComplete = true;
			}
		}

		// Small sleep to avoid busy-waiting
		FPlatformProcess::Sleep(0.1f);
	}

	// Unbind the delegate
	if (PatchCompleteHandle.IsValid())
	{
		LiveCodingModule->GetOnPatchCompleteDelegate().Remove(PatchCompleteHandle);
	}

	// Check for timeout
	if (!bCompilationComplete)
	{
		Result.status = TEXT("timeout");
		Result.error = FString::Printf(TEXT("Compilation timed out after %.1f seconds"), Params.timeoutSeconds);
		Result.bSuccess = false;
		Result.buildLog = TEXT("Compilation timed out before completion.");
	}
	else
	{
		// Compilation completed - get the final result from Live Coding
		ELiveCodingCompileResult FinalCompileResult = ELiveCodingCompileResult::NotStarted;
		// Call Compile again to get the final result (it will return the last result if not compiling)
		LiveCodingModule->Compile(ELiveCodingCompileFlags::None, &FinalCompileResult);
		
		// Handle the completion
		return HandleCompilationComplete(LiveCodingModule, FinalCompileResult, Result, Content);
	}

	// Convert USTRUCT to JSON string at the end
	if (!UMCP_ToJsonString(Result, Content.text))
	{
		Content.text = TEXT("Failed to serialize result");
		return false;
	}

	UE_LOG(LogUnrealMCPServer, Log, TEXT("RequestEditorCompile: Compilation %s (status: %s, errors: %d, warnings: %d)"), 
		Result.bSuccess ? TEXT("succeeded") : TEXT("failed"), *Result.status, Result.errors.Num(), Result.warnings.Num());

	return true;
}

bool FUMCP_CommonTools::HandleCompilationComplete(ILiveCodingModule* LiveCodingModule, ELiveCodingCompileResult CompileResult, FUMCP_RequestEditorCompileResult& Result, FUMCP_CallToolResultContent& Content)
{
	// Give a small delay to ensure log files are written
	FPlatformProcess::Sleep(0.5f);

	// Determine success based on the Live Coding compile result
	bool bCompilationSucceeded = false;
	if (CompileResult == ELiveCodingCompileResult::Success || 
	    CompileResult == ELiveCodingCompileResult::NoChanges)
	{
		bCompilationSucceeded = true;
		Result.status = TEXT("completed");
	}
	else if (CompileResult == ELiveCodingCompileResult::Failure || 
	         CompileResult == ELiveCodingCompileResult::Cancelled)
	{
		bCompilationSucceeded = false;
		Result.status = TEXT("failed");
	}
	else
	{
		// Unknown result state - treat as failed
		bCompilationSucceeded = false;
		Result.status = TEXT("failed");
	}
	
	Result.bSuccess = bCompilationSucceeded;

	// Get the Unreal Build Tool log file path
	FString UBTLogFilePath = FPaths::Combine(FPaths::EngineDir(), TEXT("Programs"), TEXT("UnrealBuildTool"), TEXT("Log.txt"));
	UBTLogFilePath = FPaths::ConvertRelativePathToFull(UBTLogFilePath);

	// Read the UBT log file to get detailed compiler errors
	FString UBTLogContent;
	if (FPaths::FileExists(UBTLogFilePath))
	{
		// Read the last portion of the log file (last 50KB or so to get recent compilation output)
		int64 LogFileSize = IFileManager::Get().FileSize(*UBTLogFilePath);
		int64 BytesToRead = FMath::Min(LogFileSize, static_cast<int64>(50000)); // Read last 50KB
		
		if (BytesToRead > 0)
		{
			TUniquePtr<FArchive> FileReader = TUniquePtr<FArchive>(IFileManager::Get().CreateFileReader(*UBTLogFilePath));
			if (FileReader.IsValid())
			{
				// Seek to the end minus the bytes we want to read
				FileReader->Seek(LogFileSize - BytesToRead);
				TArray<uint8> Buffer;
				Buffer.SetNumUninitialized(BytesToRead);
				FileReader->Serialize(Buffer.GetData(), BytesToRead);
				FileReader->Close();
				
				// Convert to string
				FFileHelper::BufferToString(UBTLogContent, Buffer.GetData(), Buffer.Num());
			}
		}
		else
		{
			// File is small, read it all
			FFileHelper::LoadFileToString(UBTLogContent, *UBTLogFilePath);
		}
	}

	// Parse errors and warnings from the UBT log
	TArray<FString> LogLines;
	UBTLogContent.ParseIntoArrayLines(LogLines, false);
	
	for (const FString& Line : LogLines)
	{
		FString LineUpper = Line.ToUpper();
		// Check for compiler error indicators (case-insensitive)
		// Look for patterns like "error C", "fatal error", "error:", etc.
		if (LineUpper.Contains(TEXT("ERROR C")) || 
		    LineUpper.Contains(TEXT("FATAL ERROR")) || 
		    LineUpper.Contains(TEXT(": ERROR:")) ||
		    (LineUpper.Contains(TEXT("ERROR")) && (LineUpper.Contains(TEXT(".CPP")) || LineUpper.Contains(TEXT(".H")))))
		{
			Result.errors.Add(Line);
		}
		// Check for warning indicators (case-insensitive)
		else if (LineUpper.Contains(TEXT("WARNING C")) || 
		         LineUpper.Contains(TEXT(": WARNING:")) ||
		         (LineUpper.Contains(TEXT("WARNING")) && (LineUpper.Contains(TEXT(".CPP")) || LineUpper.Contains(TEXT(".H")))))
		{
			Result.warnings.Add(Line);
		}
	}

	// Set the build log content
	Result.buildLog = UBTLogContent;

	// Set error message if compilation failed
	if (!bCompilationSucceeded && Result.error.IsEmpty())
	{
		if (Result.errors.Num() > 0)
		{
			Result.error = FString::Printf(TEXT("Compilation failed with %d error(s)"), Result.errors.Num());
		}
		else
		{
			// Map compile result to error message
			switch (CompileResult)
			{
			case ELiveCodingCompileResult::Failure:
				Result.error = TEXT("Compilation failed. Check build log for details.");
				break;
			case ELiveCodingCompileResult::Cancelled:
				Result.error = TEXT("Compilation was cancelled.");
				break;
			default:
				Result.error = TEXT("Compilation completed but may have failed. Check build log for details.");
				break;
			}
		}
	}

	// Convert USTRUCT to JSON string
	if (!UMCP_ToJsonString(Result, Content.text))
	{
		Content.text = TEXT("Failed to serialize result");
		return false;
	}

	return true;
}
