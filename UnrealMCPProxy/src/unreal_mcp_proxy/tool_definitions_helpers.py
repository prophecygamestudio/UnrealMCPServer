"""Shared helpers for tool definition modules.

Provides utilities for conditional markdown export support and other shared functionality.
"""

from typing import Tuple


def get_markdown_helpers(enable_markdown_export: bool) -> Tuple[str, str]:
    """Get markdown-related helper strings for tool descriptions.
    
    Args:
        enable_markdown_export: Whether markdown export is enabled
    
    Returns:
        Tuple of (markdown_support, markdown_format) strings
    """
    markdown_support = "Markdown format provides structured documentation of asset properties. " if enable_markdown_export else ""
    markdown_format = ", 'md'" if enable_markdown_export else ""
    return markdown_support, markdown_format

