"""Constants for UnrealMCPProxy."""

# Default port numbers
DEFAULT_BACKEND_PORT = 30069
DEFAULT_PROXY_PORT = 30070

# Default timeouts (in seconds)
DEFAULT_BACKEND_TIMEOUT = 30
DEFAULT_COMPILATION_TIMEOUT = 300.0  # 5 minutes

# Default health check interval (in seconds)
DEFAULT_HEALTH_CHECK_INTERVAL = 5

# Default retry settings
DEFAULT_RETRY_MAX_ATTEMPTS = 3
DEFAULT_RETRY_INITIAL_DELAY = 0.5
DEFAULT_RETRY_MAX_DELAY = 5.0
DEFAULT_RETRY_BACKOFF_FACTOR = 2.0

# Default export format
DEFAULT_EXPORT_FORMAT = "T3D"

# JSON-RPC version
JSON_RPC_VERSION = "2.0"

# MCP Protocol version
MCP_PROTOCOL_VERSION = "2024-11-05"

