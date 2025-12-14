"""Configuration and settings for UnrealMCPProxy."""

import logging
from enum import Enum
from pathlib import Path
from typing import Optional
from pydantic import field_validator
from pydantic_settings import BaseSettings, SettingsConfigDict

from .constants import (
    DEFAULT_BACKEND_PORT, DEFAULT_PROXY_PORT, DEFAULT_BACKEND_TIMEOUT,
    DEFAULT_HEALTH_CHECK_INTERVAL, DEFAULT_RETRY_MAX_ATTEMPTS,
    DEFAULT_RETRY_INITIAL_DELAY, DEFAULT_RETRY_MAX_DELAY,
    DEFAULT_RETRY_BACKOFF_FACTOR
)


class MCPTransport(str, Enum):
    """MCP transport types."""
    stdio = "stdio"
    sse = "sse"
    http = "http"


class ServerSettings(BaseSettings):
    """Server configuration settings."""
    model_config = SettingsConfigDict(env_prefix="unreal_mcp_proxy_", env_file=".env", extra='ignore')
    
    # Backend (Unreal MCP Server) settings
    backend_host: str = "localhost"
    backend_port: int = DEFAULT_BACKEND_PORT
    backend_timeout: int = DEFAULT_BACKEND_TIMEOUT
    health_check_interval: int = DEFAULT_HEALTH_CHECK_INTERVAL
    health_check_start_on_first_call: bool = True  # Start health check on first tool call (lazy initialization)
    
    # Retry settings for read-only operations
    retry_max_attempts: int = DEFAULT_RETRY_MAX_ATTEMPTS
    retry_initial_delay: float = DEFAULT_RETRY_INITIAL_DELAY
    retry_max_delay: float = DEFAULT_RETRY_MAX_DELAY
    retry_backoff_factor: float = DEFAULT_RETRY_BACKOFF_FACTOR
    
    # Proxy server settings
    host: str = "0.0.0.0"
    port: int = DEFAULT_PROXY_PORT
    transport: MCPTransport = MCPTransport.stdio  # Default to STDIO for multiple agent support
    
    # Conditional feature flags
    enable_markdown_export: bool = True  # Enable markdown export tools and resources (requires BP2AI plugin)
    
    # Definitions path (for prompts and resources JSON files)
    # Defaults to relative path: ../Resources (plugin root/Resources)
    # Can be set to absolute path or relative path from plugin root
    definitions_path: Optional[str] = None  # None = use default relative path
    
    # Development mode
    debug: bool = False  # Enable debug/development features
    
    # Logging
    log_level: str = "INFO"
    log_file: Optional[str] = None
    log_format: str = "%(asctime)s - %(name)s - %(levelname)s - %(message)s"
    
    @field_validator('backend_port', 'port')
    @classmethod
    def validate_port(cls, v: int) -> int:
        """Validate port is in valid range."""
        if not (1 <= v <= 65535):
            raise ValueError(f"Port must be between 1 and 65535, got {v}")
        return v
    
    @field_validator('backend_timeout', 'health_check_interval')
    @classmethod
    def validate_timeout(cls, v: int) -> int:
        """Validate timeout is positive."""
        if v <= 0:
            raise ValueError(f"Timeout must be positive, got {v}")
        return v
    
    @field_validator('retry_max_attempts')
    @classmethod
    def validate_retry_attempts(cls, v: int) -> int:
        """Validate retry attempts is non-negative."""
        if v < 0:
            raise ValueError(f"Retry max attempts must be non-negative, got {v}")
        return v
    
    @field_validator('retry_initial_delay', 'retry_max_delay', 'retry_backoff_factor')
    @classmethod
    def validate_retry_delays(cls, v: float) -> float:
        """Validate retry delay values are positive."""
        if v <= 0:
            raise ValueError(f"Retry delay must be positive, got {v}")
        return v
    
    @field_validator('transport')
    @classmethod
    def validate_transport(cls, v: str | MCPTransport) -> MCPTransport:
        """Validate transport type."""
        if isinstance(v, str):
            try:
                return MCPTransport(v.lower())
            except ValueError:
                raise ValueError(f"Invalid transport type: {v}. Must be one of: {', '.join(t.value for t in MCPTransport)}")
        return v
    
    @field_validator('log_level')
    @classmethod
    def validate_log_level(cls, v: str) -> str:
        """Validate log level."""
        valid_levels = {'DEBUG', 'INFO', 'WARNING', 'ERROR', 'CRITICAL'}
        if v.upper() not in valid_levels:
            raise ValueError(f"Invalid log level: {v}. Must be one of: {', '.join(valid_levels)}")
        return v.upper()


def setup_logging(settings: ServerSettings):
    """Configure logging based on settings."""
    # In debug mode, force DEBUG log level
    if settings.debug:
        numeric_level = logging.DEBUG
        # Override log_level if debug is enabled
        if settings.log_level.upper() != "DEBUG":
            logging.getLogger().info("Debug mode enabled - setting log level to DEBUG")
    else:
        # Convert log_level string to logging level constant
        numeric_level = getattr(logging, settings.log_level.upper(), logging.INFO)
    
    # Create formatter
    formatter = logging.Formatter(settings.log_format)
    
    # Get root logger
    root_logger = logging.getLogger()
    root_logger.setLevel(numeric_level)
    
    # Remove existing handlers to avoid duplicates
    root_logger.handlers.clear()
    
    # Add console handler (always)
    console_handler = logging.StreamHandler()
    console_handler.setLevel(numeric_level)
    console_handler.setFormatter(formatter)
    root_logger.addHandler(console_handler)
    
    # Add file handler if log_file is specified
    if settings.log_file:
        # Ensure parent directory exists
        log_path = Path(settings.log_file)
        log_path.parent.mkdir(parents=True, exist_ok=True)
        
        file_handler = logging.FileHandler(settings.log_file, encoding='utf-8')
        file_handler.setLevel(numeric_level)
        file_handler.setFormatter(formatter)
        root_logger.addHandler(file_handler)
    
    # Create module-specific logger for proxy operations
    proxy_logger = logging.getLogger("unreal_mcp_proxy")
    proxy_logger.setLevel(numeric_level)

