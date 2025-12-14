# Shared Definitions Resources

This directory contains JSON files that are shared between the UnrealMCPServer backend (C++) and the UnrealMCPProxy (Python) to ensure consistency.

## Files

- **`prompts.json`**: Prompt definitions (name, title, description, arguments, template)
- **`resources.json`**: Resource template definitions (name, description, uriTemplate, mimeType)

## Purpose

These definitions are simple metadata (strings, descriptions, templates) that must match exactly between the proxy and backend. By using shared JSON files:

1. **Single Source of Truth**: Definitions are defined once, used by both implementations
2. **No Sync Issues**: Both proxy and backend read from the same files
3. **Easy Updates**: Edit JSON files to update definitions in both systems

## Usage

### Proxy (Python)

The proxy loads these files at runtime:
- `UnrealMCPProxy/src/unreal_mcp_proxy/prompt_definitions.py` loads `prompts.json`
- `UnrealMCPProxy/src/unreal_mcp_proxy/resource_definitions.py` loads `resources.json`

**Path Resolution:**
- Default: Relative path `../Resources` from `UnrealMCPProxy/` directory (plugin root)
- Custom: Set `UNREAL_MCP_PROXY_DEFINITIONS_PATH` environment variable to specify custom path
  - Can be absolute path: `/path/to/Resources`
  - Can be relative path from plugin root: `../CustomResources`

### Backend (C++)

The backend should also load these files at runtime (implementation pending). The backend currently has hardcoded definitions in:
- `Source/UnrealMCPServer/Private/UMCP_CommonPrompts.cpp`
- `Source/UnrealMCPServer/Private/UMCP_CommonResources.cpp`

**TODO**: Update backend to load from JSON files using Unreal's JSON parsing utilities.

## File Format

### prompts.json

```json
{
  "prompts": [
    {
      "name": "prompt_name",
      "title": "Prompt Title",
      "description": "Prompt description",
      "arguments": [
        {
          "name": "arg_name",
          "description": "Argument description",
          "required": true
        }
      ],
      "template": "Template text with {placeholders} for arguments"
    }
  ]
}
```

### resources.json

```json
{
  "resources": [],
  "resourceTemplates": [
    {
      "name": "Template Name",
      "description": "Template description",
      "mimeType": "text/plain",
      "uriTemplate": "unreal+scheme://{filepath}"
    }
  ]
}
```

## Directory Structure

The Resources directory is located at the plugin root level:
```
UnrealMCPServer/
├── Resources/            <- This directory
│   ├── prompts.json
│   └── resources.json
├── Source/
├── UnrealMCPProxy/
└── ...
```

Both implementations resolve the path relative to their code locations to find these files.

