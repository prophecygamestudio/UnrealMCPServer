"""Example prompts for Unreal Engine workflows.

These prompts provide reusable templates for common Unreal Engine tasks.
They can be used to guide LLM interactions for Blueprint analysis, asset management, etc.
"""

from typing import Dict, Any, List
from fastmcp import FastMCP


def create_blueprint_analysis_prompt(mcp: FastMCP):
    """Create a prompt for analyzing Blueprint structure and functionality."""
    
    @mcp.prompt()
    def analyze_blueprint(blueprint_path: str, focus_areas: str = "all") -> List[Dict[str, Any]]:
        """Analyze a Blueprint's structure, functionality, and design patterns.
        
        Args:
            blueprint_path: The path to the Blueprint asset (e.g., '/Game/Blueprints/BP_Player')
            focus_areas: Comma-separated list of areas to focus on: 'variables', 'functions', 'events', 'graph', 'design', or 'all'
        
        Returns:
            List of prompt messages for LLM interaction
        """
        focus_list = [area.strip().lower() for area in focus_areas.split(",")]
        
        messages = [
            {
                "role": "user",
                "content": {
                    "type": "text",
                    "text": f"""Analyze the Blueprint at path '{blueprint_path}' and provide a comprehensive analysis.

Focus Areas: {focus_areas}

Please provide:
1. **Overview**: High-level description of what this Blueprint does
2. **Variables**: List and explain all variables, their types, and purposes
3. **Functions**: Document all custom functions, their parameters, return values, and logic
4. **Events**: Identify all event handlers (BeginPlay, Tick, etc.) and their purposes
5. **Graph Structure**: Describe the overall flow and key connections in the Blueprint graph
6. **Design Patterns**: Identify any design patterns used (e.g., State Machine, Component Pattern)
7. **Dependencies**: List assets and classes this Blueprint depends on
8. **Potential Issues**: Identify any potential bugs, performance issues, or design concerns
9. **Suggestions**: Provide recommendations for improvements or best practices

Use the export_blueprint_markdown tool to get the full Blueprint structure, then analyze it thoroughly."""
                }
            }
        ]
        
        return messages


def create_blueprint_refactor_prompt(mcp: FastMCP):
    """Create a prompt for refactoring Blueprints."""
    
    @mcp.prompt()
    def refactor_blueprint(blueprint_path: str, refactor_goal: str) -> List[Dict[str, Any]]:
        """Generate a refactoring plan for a Blueprint.
        
        Args:
            blueprint_path: The path to the Blueprint asset
            refactor_goal: The goal of the refactoring (e.g., 'improve performance', 'add new feature', 'simplify structure')
        
        Returns:
            List of prompt messages for LLM interaction
        """
        messages = [
            {
                "role": "user",
                "content": {
                    "type": "text",
                    "text": f"""Create a refactoring plan for the Blueprint at '{blueprint_path}'.

Refactoring Goal: {refactor_goal}

Please provide:
1. **Current State Analysis**: Analyze the current Blueprint structure
2. **Refactoring Strategy**: Outline the approach to achieve the goal
3. **Step-by-Step Plan**: Detailed steps for the refactoring
4. **Breaking Changes**: Identify any breaking changes that might affect other assets
5. **Testing Plan**: Suggest how to test the refactored Blueprint
6. **Migration Guide**: If applicable, provide a guide for migrating dependent assets

Use the export_blueprint_markdown tool to examine the current Blueprint structure."""
                }
            }
        ]
        
        return messages


def create_asset_audit_prompt(mcp: FastMCP):
    """Create a prompt for auditing project assets."""
    
    @mcp.prompt()
    def audit_assets(asset_paths: str, audit_type: str = "dependencies") -> List[Dict[str, Any]]:
        """Audit project assets for dependencies, references, or issues.
        
        Args:
            asset_paths: Comma-separated list of asset paths to audit
            audit_type: Type of audit: 'dependencies', 'references', 'unused', 'orphaned', or 'all'
        
        Returns:
            List of prompt messages for LLM interaction
        """
        asset_list = [path.strip() for path in asset_paths.split(",")]
        
        messages = [
            {
                "role": "user",
                "content": {
                    "type": "text",
                    "text": f"""Audit the following assets: {', '.join(asset_list)}

Audit Type: {audit_type}

Please provide:
1. **Asset Inventory**: List all assets and their basic information
2. **Dependency Analysis**: Map dependencies between assets (use get_asset_dependencies tool)
3. **Reference Analysis**: Identify what references each asset (use get_asset_references tool)
4. **Unused Assets**: Identify assets that are not referenced by any other asset
5. **Orphaned Assets**: Find assets with broken or missing dependencies
6. **Circular Dependencies**: Detect any circular dependency chains
7. **Recommendations**: Suggest optimizations, cleanup opportunities, or restructuring

Use the search_assets, get_asset_dependencies, and get_asset_references tools to gather information."""
                }
            }
        ]
        
        return messages


def create_blueprint_creation_prompt(mcp: FastMCP):
    """Create a prompt for creating new Blueprints."""
    
    @mcp.prompt()
    def create_blueprint(blueprint_name: str, parent_class: str, description: str) -> List[Dict[str, Any]]:
        """Generate a plan for creating a new Blueprint.
        
        Args:
            blueprint_name: Name for the new Blueprint (e.g., 'BP_PlayerController')
            parent_class: Parent class to inherit from (e.g., 'PlayerController', 'Actor', 'Pawn')
            description: Description of what the Blueprint should do
        
        Returns:
            List of prompt messages for LLM interaction
        """
        messages = [
            {
                "role": "user",
                "content": {
                    "type": "text",
                    "text": f"""Create a design plan for a new Blueprint named '{blueprint_name}' that inherits from '{parent_class}'.

Description: {description}

Please provide:
1. **Blueprint Structure**: Define the variables, functions, and events needed
2. **Component Requirements**: List any components that should be added
3. **Initialization Logic**: Outline what should happen in BeginPlay and construction
4. **Core Functionality**: Describe the main functions and their implementations
5. **Event Handlers**: Specify which events to handle and how
6. **Dependencies**: Identify other assets or classes this Blueprint will need
7. **Implementation Steps**: Step-by-step guide for creating the Blueprint in Unreal Editor
8. **Testing Checklist**: Items to test once the Blueprint is created

Use search_blueprints to find similar existing Blueprints for reference."""
                }
            }
        ]
        
        return messages


def create_performance_analysis_prompt(mcp: FastMCP):
    """Create a prompt for analyzing Blueprint performance."""
    
    @mcp.prompt()
    def analyze_performance(blueprint_path: str) -> List[Dict[str, Any]]:
        """Analyze the performance characteristics of a Blueprint.
        
        Args:
            blueprint_path: The path to the Blueprint asset
        
        Returns:
            List of prompt messages for LLM interaction
        """
        messages = [
            {
                "role": "user",
                "content": {
                    "type": "text",
                    "text": f"""Analyze the performance of the Blueprint at '{blueprint_path}'.

Please provide:
1. **Performance Hotspots**: Identify nodes or functions that might cause performance issues
2. **Tick Analysis**: Review Tick event usage and suggest optimizations
3. **Memory Usage**: Analyze variable usage and memory footprint
4. **Event Frequency**: Identify frequently called events and their impact
5. **Optimization Opportunities**: Suggest specific optimizations (e.g., caching, batching, reducing tick frequency)
6. **Best Practices**: Recommend performance best practices for this Blueprint
7. **Profiling Recommendations**: Suggest what to profile in Unreal's profiler

Use export_blueprint_markdown to examine the Blueprint structure, then analyze it for performance concerns."""
                }
            }
        ]
        
        return messages


def register_all_prompts(mcp: FastMCP):
    """Register all example prompts with the FastMCP server.
    
    Note: These prompts are examples. The actual prompts will come from the backend.
    This function is kept for reference but prompts should be registered by the backend.
    """
    # Prompts are registered via decorators in the functions above
    # In practice, prompts should be registered by the backend server
    # This function is a placeholder for documentation
    pass

