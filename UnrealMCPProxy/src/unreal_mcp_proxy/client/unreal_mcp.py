"""UnrealMCPClient - Client for communicating with Unreal Engine MCP Server."""

import asyncio
import json
import logging
from enum import Enum
from typing import Optional, Dict, Any
import httpx
from pydantic_settings import BaseSettings, SettingsConfigDict

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
    port: int = 30069
    timeout: int = 30


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
        
        # Test connection on initialization (synchronous check)
        self._test_connection()
        
        # Start background health check task (deferred until event loop is available)
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
    
    async def initialize_async(self):
        """Initialize async components (health check) when event loop is available.
        
        Call this method after the event loop is running to start the health check.
        """
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
            
            # Wait before next check (use settings if available)
            await asyncio.sleep(5)  # Default 5 seconds
    
    def _test_connection(self) -> bool:
        """Test if the Unreal MCP server is reachable (synchronous, for initial check).
        
        Returns:
            True if server is reachable, False otherwise.
        """
        test_url = f"http://{self.settings.host}:{self.settings.port}/mcp"
        
        try:
            # Try a simple ping request
            request = {
                "jsonrpc": "2.0",
                "id": 1,
                "method": "ping",
                "params": {}
            }
            
            # Use synchronous request for connection test
            response = httpx.post(test_url, json=request, timeout=5)
            
            if response.status_code == 200:
                result = response.json()
                if "result" in result or "error" in result:
                    logger.info(f"Successfully connected to Unreal MCP server at {test_url}")
                    self.state = ConnectionState.ONLINE
                    import time
                    self.last_known_good_connection = time.time()
                    return True
            
            logger.warning(f"Unreal MCP server at {test_url} returned unexpected response: {response.status_code}")
            self.state = ConnectionState.OFFLINE
            return False
            
        except httpx.ConnectError as e:
            logger.warning(f"Failed to connect to Unreal MCP server at {test_url}: {e}")
            self.state = ConnectionState.OFFLINE
            return False
        except httpx.TimeoutException:
            logger.warning(f"Connection timeout to Unreal MCP server at {test_url}")
            self.state = ConnectionState.OFFLINE
            return False
        except Exception as e:
            logger.error(f"Unexpected error while testing connection to Unreal MCP server at {test_url}: {str(e)}", exc_info=True)
            self.state = ConnectionState.OFFLINE
            return False
    
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
        """
        if self.state == ConnectionState.OFFLINE:
            # Try to reconnect
            if not self._test_connection():
                raise ConnectionError(f"Unreal MCP server is not available at {self.base_url}")
        
        if request_id is None:
            import random
            request_id = random.randint(1, 2**31 - 1)
        
        request = {
            "jsonrpc": "2.0",
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
    
    async def get_resources_list(self) -> Dict[str, Any]:
        """Get the list of resources from the backend server.
        
        Returns:
            Response dictionary with resources list.
        """
        return await self.call_method("resources/list", params={})
    
    async def get_resource_templates_list(self) -> Dict[str, Any]:
        """Get the list of resource templates from the backend server.
        
        Returns:
            Response dictionary with resource templates list.
        """
        return await self.call_method("resources/templates/list", params={})
    
    async def read_resource(self, uri: str) -> Dict[str, Any]:
        """Read a resource from the backend server.
        
        Args:
            uri: Resource URI to read
        
        Returns:
            Response dictionary with resource contents.
        """
        return await self.call_method("resources/read", params={"uri": uri})
    
    async def get_prompts_list(self) -> Dict[str, Any]:
        """Get the list of prompts from the backend server.
        
        Returns:
            Response dictionary with prompts list.
        """
        return await self.call_method("prompts/list", params={})
    
    async def get_prompt(self, name: str, arguments: Optional[Dict[str, Any]] = None) -> Dict[str, Any]:
        """Get a prompt from the backend server.
        
        Args:
            name: Prompt name
            arguments: Optional prompt arguments
        
        Returns:
            Response dictionary with prompt messages.
        """
        return await self.call_method("prompts/get", params={
            "name": name,
            "arguments": arguments or {}
        })
    
    async def initialize(self, protocol_version: str = "2024-11-05") -> Dict[str, Any]:
        """Initialize connection with the backend server.
        
        Args:
            protocol_version: MCP protocol version
        
        Returns:
            Response dictionary with server capabilities.
        """
        return await self.call_method("initialize", params={
            "protocolVersion": protocol_version
        })
    
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

