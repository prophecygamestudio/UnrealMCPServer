"""Initialization utilities for UnrealMCPProxy."""

import asyncio
import logging
from typing import Optional, Dict, Any

from .config import ServerSettings
from .client.unreal_mcp import UnrealMCPClient, UnrealMCPSettings, ConnectionState
from .compatibility import check_tool_compatibility
from .tool_definitions import get_tool_definitions

logger = logging.getLogger(__name__)


async def initialize_proxy(
    settings: ServerSettings,
    client: UnrealMCPClient,
    cached_proxy_tool_definitions: Optional[Dict[str, Dict[str, Any]]]
) -> None:
    """Initialize proxy components (health check, tool definitions, compatibility checking).
    
    This function orchestrates the startup sequence:
    1. Starts health check loop
    2. Loads tool definitions if not already loaded
    3. Checks compatibility when backend comes online
    
    Args:
        settings: Server settings
        client: UnrealMCPClient instance
        cached_proxy_tool_definitions: Optional cached tool definitions (will be loaded if None)
    """
    logger.info("Initializing proxy components...")
    
    # Start health check if not already started
    if not client._health_check_started:
        try:
            await client.initialize_async()
            logger.debug("Health check started")
        except RuntimeError:
            logger.warning("No event loop available - health check will start when event loop is available")
    
    # Load tool definitions if not already loaded
    if cached_proxy_tool_definitions is None:
        logger.info("Loading proxy tool definitions...")
        tool_defs = get_tool_definitions(enable_markdown_export=settings.enable_markdown_export)
        logger.info(f"Loaded {len(tool_defs)} proxy tool definitions")
        # Note: This function doesn't modify the passed dict, caller should handle assignment
    
    # Check compatibility when backend comes online (async, non-blocking)
    async def check_compatibility_when_online():
        """Check compatibility when backend comes online."""
        # Wait a bit for health check to establish connection
        await asyncio.sleep(1)
        if client.state == ConnectionState.ONLINE:
            await check_tool_compatibility(client, cached_proxy_tool_definitions)
    
    # Start compatibility check task (non-blocking)
    try:
        asyncio.create_task(check_compatibility_when_online())
    except RuntimeError:
        # No event loop yet, will be checked on first tool call
        logger.debug("No event loop available - compatibility check will run on first tool call")
    
    logger.info("Proxy initialization complete")

