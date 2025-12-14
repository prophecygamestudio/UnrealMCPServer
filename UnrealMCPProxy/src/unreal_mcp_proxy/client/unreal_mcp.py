"""UnrealMCPClient - Client for communicating with Unreal Engine MCP Server."""

import asyncio
import json
import logging
from enum import Enum
from typing import Optional, Dict, Any
import httpx
from pydantic import field_validator
from pydantic_settings import BaseSettings, SettingsConfigDict

from ..constants import DEFAULT_BACKEND_PORT, DEFAULT_BACKEND_TIMEOUT, DEFAULT_HEALTH_CHECK_INTERVAL

logger = logging.getLogger(__name__)


class ConnectionState(str, Enum):
    """Connection state to backend server."""
    UNKNOWN = "unknown"
    ONLINE = "online"
    OFFLINE = "offline"


class UnrealMCPSettings(BaseSettings):
    """Settings for Unreal MCP Server connection."""
    model_config = SettingsConfigDict(
        env_prefix="unreal_mcp_proxy_backend_", 
        env_file=".env", 
        extra='ignore',
        case_sensitive=False
    )
    
    host: str = "localhost"
    port: int = DEFAULT_BACKEND_PORT
    timeout: int = DEFAULT_BACKEND_TIMEOUT
    health_check_interval: int = DEFAULT_HEALTH_CHECK_INTERVAL
    
    @field_validator('port')
    @classmethod
    def validate_port(cls, v: int) -> int:
        """Validate port is in valid range."""
        if not (1 <= v <= 65535):
            raise ValueError(f"Port must be between 1 and 65535, got {v}")
        return v
    
    @field_validator('timeout', 'health_check_interval')
    @classmethod
    def validate_timeout(cls, v: int) -> int:
        """Validate timeout is positive."""
        if v <= 0:
            raise ValueError(f"Timeout must be positive, got {v}")
        return v


class UnrealMCPClient:
    """Client for communicating with Unreal Engine MCP Server."""
    
    def __init__(self, settings: Optional[UnrealMCPSettings] = None):
        """Initialize the Unreal MCP client.
        
        Args:
            settings: Optional settings. If not provided, loads from environment.
        """
        self.settings = settings or UnrealMCPSettings()
        self.base_url = f"http://{self.settings.host}:{self.settings.port}/mcp"
        self.state = ConnectionState.UNKNOWN
        self.last_known_good_connection: Optional[float] = None
        
        # Create HTTP client with timeout
        self.client = httpx.AsyncClient(
            timeout=httpx.Timeout(self.settings.timeout),
            base_url=self.base_url
        )
        
        logger.info(f"UnrealMCPClient initialized: {self.base_url}")
        
        # Start background health check task (deferred until event loop is available)
        # The health check loop will determine connection state asynchronously
        self._health_check_task = None
        self._health_check_started = False
    
    def _start_health_check(self):
        """Start the background health check task.
        
        This method checks if there's a running event loop before creating the task.
        If no loop is running, the health check will be started later when initialize_async() is called.
        """
        # Check if there's a running event loop
        try:
            loop = asyncio.get_running_loop()
        except RuntimeError:
            # No running loop - defer health check start
            logger.debug("No running event loop, health check will start when event loop is available")
            return
        
        # We have a running loop, start the health check
        if self._health_check_task is None:
            self._health_check_task = asyncio.create_task(self._health_check_loop())
            self._health_check_started = True
            logger.debug("Started background health check task")
        elif self._health_check_task.done():
            # Task completed, restart it
            self._health_check_task = asyncio.create_task(self._health_check_loop())
            self._health_check_started = True
            logger.debug("Restarted background health check task")
    
    async def _check_connection_immediate(self) -> bool:
        """Check connection state immediately (synchronous within async context).
        
        This performs a quick connection test to determine initial state.
        Uses a shorter timeout to avoid blocking too long.
        
        Returns:
            True if backend is online, False otherwise
        """
        try:
            # Use a shorter timeout for initial check (2 seconds)
            import httpx
            from ..constants import JSON_RPC_VERSION
            
            quick_timeout = httpx.Timeout(2.0)  # 2 second timeout for initial check
            async with httpx.AsyncClient(timeout=quick_timeout, base_url=self.base_url) as quick_client:
                request = {
                    "jsonrpc": JSON_RPC_VERSION,
                    "id": 1,
                    "method": "tools/list",
                    "params": {}
                }
                response = await quick_client.post("", json=request)
                response.raise_for_status()
                result = response.json()
                # If we got a response (even with error), backend is online
                return True
        except (httpx.ConnectError, httpx.TimeoutException):
            # Connection failed - backend is offline
            return False
        except Exception as e:
            # Other errors might indicate backend is online but had an issue
            # If we got any response, backend is online
            logger.debug(f"Initial connection check had error but got response: {str(e)}")
            return True
    
    async def initialize_async(self):
        """Initialize async components (health check) when event loop is available.
        
        Performs an immediate connection check to set initial state, then starts
        the background health check loop.
        
        Call this method after the event loop is running to start the health check.
        """
        # First, do an immediate connection check to set initial state
        if self.state == ConnectionState.UNKNOWN:
            logger.debug("Performing immediate connection check...")
            is_online = await self._check_connection_immediate()
            if is_online:
                self.state = ConnectionState.ONLINE
                import time
                self.last_known_good_connection = time.time()
                logger.info("Backend connection verified on initialization")
            else:
                self.state = ConnectionState.OFFLINE
                logger.debug("Backend appears offline on initialization")
        
        # Then start the background health check loop
        if not self._health_check_started:
            self._start_health_check()
    
    async def _health_check_loop(self):
        """Background health check loop that periodically tests connection."""
        while True:
            try:
                # Test connection with ping
                response = await self.call_method("ping", {})
                if "result" in response:
                    if self.state != ConnectionState.ONLINE:
                        logger.info("Backend connection established")
                    self.state = ConnectionState.ONLINE
                else:
                    if self.state != ConnectionState.OFFLINE:
                        logger.warning("Backend connection lost")
                    self.state = ConnectionState.OFFLINE
            except Exception as e:
                if self.state != ConnectionState.OFFLINE:
                    logger.warning(f"Backend health check failed: {str(e)}")
                self.state = ConnectionState.OFFLINE
            
            # Wait before next check (use interval from settings)
            await asyncio.sleep(self.settings.health_check_interval)
    
    async def call_method(self, method: str, params: Optional[Dict[str, Any]] = None, request_id: Optional[int] = None) -> Dict[str, Any]:
        """Call an MCP method on the backend server.
        
        Args:
            method: The MCP method name (e.g., "tools/list", "tools/call")
            params: Optional parameters for the method
            request_id: Optional request ID (auto-generated if not provided)
        
        Returns:
            Response dictionary with "result" or "error" key
        
        Raises:
            httpx.HTTPError: If the HTTP request fails
            httpx.TimeoutException: If the request times out
            ConnectionError: If the server is not available
        """
        # Note: We don't check state here - let the async HTTP call handle connection errors.
        # The health check loop keeps the state updated, and the HTTP client will raise
        # appropriate exceptions if the connection fails.
        
        if request_id is None:
            import random
            request_id = random.randint(1, 2**31 - 1)
        
        from ..constants import JSON_RPC_VERSION
        
        request = {
            "jsonrpc": JSON_RPC_VERSION,
            "id": request_id,
            "method": method,
            "params": params or {}
        }
        
        logger.debug(f"Calling backend method '{method}' with request_id={request_id}")
        
        try:
            response = await self.client.post("", json=request)
            response.raise_for_status()
            
            result = response.json()
            
            # Check for JSON-RPC errors
            if "error" in result:
                error = result["error"]
                logger.error(
                    f"Backend returned error for method '{method}': "
                    f"code={error.get('code')}, message={error.get('message')}"
                )
            else:
                logger.debug(f"Backend method '{method}' succeeded")
            
            # Update connection state on successful response
            self.state = ConnectionState.ONLINE
            import time
            self.last_known_good_connection = time.time()
            
            return result
            
        except httpx.ConnectError as e:
            logger.error(f"Connection error calling backend method '{method}': {str(e)}", exc_info=True)
            self.state = ConnectionState.OFFLINE
            raise ConnectionError(f"Failed to connect to Unreal MCP server: {str(e)}")
        except httpx.TimeoutException as e:
            logger.warning(f"Timeout calling backend method '{method}' (timeout={self.settings.timeout}s)")
            self.state = ConnectionState.OFFLINE
            raise TimeoutError(f"Request to Unreal MCP server timed out: {str(e)}")
        except httpx.HTTPError as e:
            logger.error(f"HTTP error calling backend method '{method}': {str(e)}", exc_info=True)
            self.state = ConnectionState.OFFLINE
            raise
        except Exception as e:
            logger.error(f"Unexpected error calling backend method '{method}': {str(e)}", exc_info=True)
            self.state = ConnectionState.OFFLINE
            raise
    
    async def ping(self) -> bool:
        """Ping the backend server to check if it's available.
        
        Returns:
            True if server responds, False otherwise.
        """
        try:
            result = await self.call_method("ping")
            return "error" not in result
        except Exception as e:
            logger.debug(f"Ping failed: {str(e)}")
            return False
    
    async def get_tools_list(self) -> Dict[str, Any]:
        """Get the list of tools from the backend server.
        
        Returns:
            Response dictionary with tools list.
        """
        return await self.call_method("tools/list", params={})
    
    async def call_tool(self, tool_name: str, arguments: Dict[str, Any]) -> Dict[str, Any]:
        """Call a tool on the backend server.
        
        Args:
            tool_name: Name of the tool to call
            arguments: Tool arguments
        
        Returns:
            Response dictionary with tool result.
        """
        return await self.call_method("tools/call", params={
            "name": tool_name,
            "arguments": arguments
        })
    
    async def get_resources_list(self, cursor: Optional[str] = None) -> Dict[str, Any]:
        """Get the list of resources from the backend server.
        
        Args:
            cursor: Optional cursor for pagination
        
        Returns:
            Response dictionary with resources list.
        """
        params = {}
        if cursor:
            params["cursor"] = cursor
        return await self.call_method("resources/list", params=params)
    
    async def get_resource_templates_list(self, cursor: Optional[str] = None) -> Dict[str, Any]:
        """Get the list of resource templates from the backend server.
        
        Args:
            cursor: Optional cursor for pagination
        
        Returns:
            Response dictionary with resource templates list.
        """
        params = {}
        if cursor:
            params["cursor"] = cursor
        return await self.call_method("resources/templates/list", params=params)
    
    async def read_resource(self, uri: str) -> Dict[str, Any]:
        """Read a resource from the backend server.
        
        Args:
            uri: The URI of the resource to read
        
        Returns:
            Response dictionary with resource content.
        """
        return await self.call_method("resources/read", params={"uri": uri})
    
    async def get_prompts_list(self, cursor: Optional[str] = None) -> Dict[str, Any]:
        """Get the list of prompts from the backend server.
        
        Args:
            cursor: Optional cursor for pagination
        
        Returns:
            Response dictionary with prompts list.
        """
        params = {}
        if cursor:
            params["cursor"] = cursor
        return await self.call_method("prompts/list", params=params)
    
    async def get_prompt(self, name: str, arguments: Optional[Dict[str, Any]] = None) -> Dict[str, Any]:
        """Get a prompt from the backend server.
        
        Args:
            name: Name of the prompt
            arguments: Optional arguments for the prompt
        
        Returns:
            Response dictionary with prompt messages.
        """
        params = {"name": name}
        if arguments:
            params["arguments"] = arguments
        return await self.call_method("prompts/get", params=params)
    
    async def shutdown(self):
        """Shutdown the client and cancel background tasks."""
        # Cancel health check task if running
        if self._health_check_task is not None and not self._health_check_task.done():
            self._health_check_task.cancel()
            try:
                await self._health_check_task
            except asyncio.CancelledError:
                pass
            self._health_check_task = None
            self._health_check_started = False
            logger.debug("Health check task cancelled")
        
        # Close HTTP client
        await self.client.aclose()
        logger.info("UnrealMCPClient closed")
    
    async def close(self):
        """Close the HTTP client connection."""
        await self.shutdown()

