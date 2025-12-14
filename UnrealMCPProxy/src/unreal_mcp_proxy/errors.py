"""Error handling utilities for UnrealMCPProxy."""

import json
from typing import Dict, Any, Optional


def create_error_response(error_message: str, error_code: Optional[str] = None) -> Dict[str, Any]:
    """Create a standardized error response in MCP format.
    
    Args:
        error_message: Human-readable error message
        error_code: Optional error code for programmatic handling
    
    Returns:
        Error response dictionary in MCP format
    """
    error_data = {"error": error_message}
    if error_code:
        error_data["code"] = error_code
    
    return {
        "isError": True,
        "content": [{
            "type": "text",
            "text": json.dumps(error_data)
        }]
    }

