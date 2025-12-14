"""Common Tools definitions for Unreal MCP Server.

These definitions correspond to FUMCP_CommonTools in the backend.
MUST be kept in sync with UnrealMCPServer plugin Common Tools.

See .cursorrules for synchronization requirements.
"""

from typing import Dict, Any
from .tool_definitions_helpers import get_markdown_helpers


def get_common_tools(enable_markdown_export: bool = True) -> Dict[str, Dict[str, Any]]:
    """Get Common Tools definitions.
    
    Args:
        enable_markdown_export: Whether to include markdown export support in descriptions
    
    Returns:
        Dictionary mapping tool names to tool definitions
    """
    tools = {}
    markdown_support, markdown_format = get_markdown_helpers(enable_markdown_export)
    
    # get_project_config
    tools["get_project_config"] = {
        "name": "get_project_config",
        "description": "Retrieve project and engine configuration information including engine version, directory paths (Engine, Project, Content, Log, Saved, Config, Plugins), and other essential project metadata. Use this tool first to understand the project structure before performing asset operations. Returns absolute paths that can be used in other tool calls.",
        "inputSchema": {
            "type": "object",
            "properties": {}
        },
        "outputSchema": {
            "type": "object",
            "properties": {
                "engineVersion": {
                    "type": "object",
                    "description": "Engine version information",
                    "default": {
                        "changelist": 0,
                        "full": "",
                        "major": 0,
                        "minor": 0,
                        "patch": 0
                    }
                },
                "paths": {
                    "type": "object",
                    "description": "Project and engine directory paths",
                    "default": {
                        "engineContentDir": "",
                        "engineDir": "",
                        "enginePluginsDir": "",
                        "projectConfigDir": "",
                        "projectContentDir": "",
                        "projectDir": "",
                        "projectLogDir": "",
                        "projectPluginsDir": "",
                        "projectSavedDir": ""
                    }
                }
            },
            "required": ["engineVersion", "paths"]
        }
    }
    
    # execute_console_command
    tools["execute_console_command"] = {
        "name": "execute_console_command",
        "description": "Execute an Unreal Engine console command and return its output. This allows running any console command available in the Unreal Engine editor. Common commands: 'stat fps' (performance stats), 'showdebug ai' (AI debugging), 'r.SetRes 1920x1080' (set resolution), 'open /Game/Maps/MainLevel' (load level), 'stat unit' (frame timing). Note: Some commands modify editor state. Returns command output as a string. Some commands may return empty strings if they only produce visual output in the editor.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "command": {
                    "type": "string",
                    "description": "The console command to execute. Examples: 'stat fps' (performance), 'showdebug ai' (AI debugging), 'r.SetRes 1920x1080' (resolution), 'open /Game/Maps/MainLevel' (load level), 'stat unit' (frame timing), 'quit' (exit editor). Warning: Some commands can modify the editor state or project. Use with caution for commands that modify assets or project settings."
                }
            },
            "required": ["command"]
        },
        "outputSchema": {
            "type": "object",
            "properties": {
                "bSuccess": {"type": "boolean", "description": "Whether the command was executed successfully", "default": False},
                "command": {"type": "string", "description": "The command that was executed"},
                "output": {"type": "string", "description": "The output from the console command (if any)"},
                "error": {"type": "string", "description": "Error message if bSuccess is false"}
            },
            "required": ["bSuccess", "command"]
        }
    }
    
    # get_log_file_path
    tools["get_log_file_path"] = {
        "name": "get_log_file_path",
        "description": "Returns the absolute path of the Unreal Engine log file. Use this to locate log files for debugging. Log files are plain text and can be read with standard file reading tools. Note: The log file path changes when the editor restarts. Call this tool when you need the current log file location.",
        "inputSchema": {
            "type": "object",
            "properties": {}
        },
        "outputSchema": {
            "type": "object",
            "properties": {
                "logFilePath": {"type": "string", "description": "The full path to the Unreal Engine log file"}
            },
            "required": ["logFilePath"]
        }
    }
    
    # request_editor_compile
    tools["request_editor_compile"] = {
        "name": "request_editor_compile",
        "description": "Requests an editor compilation, waits for completion, and returns whether it succeeded or failed along with any build log generated. Use this after modifying C++ source files to recompile code changes without restarting the editor. Only works if the project has C++ code and live coding is enabled in editor settings. Default timeout is 300 seconds (5 minutes). Compilation may take longer for large projects. Returns success status, build log, and extracted errors/warnings. Check the build log for compilation errors if compilation fails.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "timeoutSeconds": {
                    "type": "number",
                    "description": "Optional timeout in seconds for waiting for compilation to complete. Default: 300 seconds (5 minutes). For large projects, you may need to increase this value. Compilation will be cancelled if it exceeds this timeout.",
                    "default": 300
                }
            },
            "required": ["timeoutSeconds"]
        },
        "outputSchema": {
            "type": "object",
            "properties": {
                "bSuccess": {"type": "boolean", "description": "Whether the compilation completed successfully", "default": False},
                "bCompileStarted": {"type": "boolean", "description": "Whether compilation was successfully initiated", "default": False},
                "status": {"type": "string", "description": "Compilation status: 'completed', 'failed', 'timeout', 'not_available', or 'error'"},
                "buildLog": {"type": "string", "description": "Full build log output from the compilation"},
                "errors": {"type": "array", "items": {"type": "string"}, "description": "Array of extracted error messages from the build log", "default": []},
                "warnings": {"type": "array", "items": {"type": "string"}, "description": "Array of extracted warning messages from the build log", "default": []},
                "error": {"type": "string", "description": "Error message if bSuccess is false or status is 'error'"}
            },
            "required": ["bSuccess", "bCompileStarted", "status"]
        }
    }
    
    return tools

