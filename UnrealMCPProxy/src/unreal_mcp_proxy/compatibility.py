"""Tool compatibility checking for UnrealMCPProxy."""

import json
import logging
from typing import Dict, Any, Optional

from .client.unreal_mcp import UnrealMCPClient, ConnectionState
from .tool_definitions import compare_tool_definitions

logger = logging.getLogger(__name__)


async def check_tool_compatibility(
    client: UnrealMCPClient,
    cached_proxy_tool_definitions: Optional[Dict[str, Dict[str, Any]]]
):
    """Check compatibility of proxy tool definitions with backend tool definitions.
    
    Tools are already registered at module load time.
    This function checks for compatibility issues when the backend is online.
    
    Args:
        client: The UnrealMCPClient instance
        cached_proxy_tool_definitions: Dictionary of proxy tool definitions
    """
    logger.info("Checking tool definition compatibility")
    
    # Try to get tools from backend and compare
    if client.state == ConnectionState.ONLINE:
        try:
            response = await client.get_tools_list()
            
            if "error" in response:
                logger.warning(f"Backend returned error when listing tools: {response['error']}")
                logger.info("Using proxy tool definitions (offline mode)")
                return
            
            backend_tools = response.get("result", {}).get("tools", [])
            
            # Check compatibility of each backend tool with proxy tool definition
            compatibility_issues_found = 0
            for backend_tool in backend_tools:
                tool_name = backend_tool.get("name")
                if not tool_name:
                    continue
                
                proxy_tool_definition = cached_proxy_tool_definitions.get(tool_name) if cached_proxy_tool_definitions else None
                if proxy_tool_definition:
                    issues = compare_tool_definitions(proxy_tool_definition, backend_tool)
                    if issues:
                        compatibility_issues_found += 1
                        # Log detailed issues for debugging
                        # Always log the first compatibility issue in detail to help debugging
                        if compatibility_issues_found == 1:
                            logger.warning(f"=== SCHEMA COMPATIBILITY ISSUE FOR '{tool_name}' ===")
                            logger.warning(f"Proxy inputSchema:\n{json.dumps(proxy_tool_definition.get('inputSchema', {}), indent=2, sort_keys=True)}")
                            logger.warning(f"Backend inputSchema:\n{json.dumps(backend_tool.get('inputSchema', {}), indent=2, sort_keys=True)}")
                            logger.warning("=" * 60)
                        logger.warning(
                            f"Schema compatibility issue detected for '{tool_name}': "
                            f"{', '.join(issues)}. "
                            f"Please update UnrealMCPProxy/src/unreal_mcp_proxy/tool_definitions.py "
                            f"to fix compatibility issues. Note: Proxy schemas can differ from backend "
                            f"as long as required fields are present and types are compatible."
                        )
                else:
                    logger.warning(
                        f"Tool '{tool_name}' found in backend but not in proxy tool definitions. "
                        f"Please add it to tool_definitions.py"
                    )
            
            if compatibility_issues_found == 0:
                logger.info(f"Tool discovery completed: {len(backend_tools)} tools found, all compatible")
            else:
                logger.warning(f"Tool discovery completed: {len(backend_tools)} tools found, {compatibility_issues_found} compatibility issue(s) detected")
            
        except Exception as e:
            logger.error(f"Error during tool discovery: {str(e)}", exc_info=True)
            logger.info("Using proxy tool definitions due to error")
    else:
        logger.info("Backend is offline, using proxy tool definitions")

