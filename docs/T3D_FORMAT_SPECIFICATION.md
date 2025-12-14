# T3D File Format Specification

## Overview

T3D (Unreal Text File) is a text-based, human-readable format used by Unreal Engine to represent map objects, actors, brushes, and other level components. T3D files enable the export and import of Unreal Engine assets in a format that can be version-controlled, parsed, and manipulated programmatically.

**MIME Type:** `application/vnd.unreal.t3d`

**File Extension:** `.t3d`

**⚠️ CRITICAL: Default Value Omission**
- **Export**: Properties matching class defaults are **omitted** from T3D files
- **Import/Parsing**: Omitted properties must be resolved to class default values
- This is a fundamental aspect of the format - see [Default Value Resolution](#default-value-resolution) section

## Purpose

T3D files are primarily used for:
- Exporting Unreal Engine assets (Blueprints, Actors, Maps, etc.) to text format
- Version control of level data
- Programmatic manipulation of Unreal Engine objects
- Transferring assets between projects
- AI agent interpretation and analysis of Unreal Engine assets

## File Structure

T3D files use a hierarchical, block-based structure with `Begin` and `End` keywords to delimit sections. The format is case-sensitive and uses specific syntax for properties and values.

### General Syntax

```
Begin <BlockType> [Attributes]
    <Properties>
    <NestedBlocks>
End <BlockType>
```

### Top-Level Structure

T3D files can have different top-level structures depending on what is being exported:

**For Map/Level Exports:**
```
Begin Map Name=<MapName>
    Begin Level NAME=<LevelName>
        <Actor Definitions>
    End Level
End Map
```

**For Blueprint Asset Exports:**
```
Begin Object Class=/Script/Engine.Blueprint Name="<BlueprintName>" ExportPath="<FullPath>"
    <Blueprint Properties>
    <Graph Definitions>
End Object
```

**For Class Default Object Exports:**
```
Begin Object Class=/Script/Engine.<ClassName> Name="Default__<ClassName>" ExportPath="<FullPath>"
    <Default Properties>
End Object
```

**Note:** When exporting individual assets (Blueprints, class defaults, etc.) via Unreal Engine's exporter, the format uses `Begin Object` instead of `Begin Map`. The `Begin Map` structure is used when exporting entire levels/maps.

## Core Block Types

### Map Block

The top-level container for all map data.

**Syntax:**
```
Begin Map Name=<MapName>
    <Level Blocks>
End Map
```

**Attributes:**
- `Name` (required): The name of the map

**Example:**
```
Begin Map Name=MyMap
    Begin Level NAME=PersistentLevel
    End Level
End Map
```

### Level Block

Represents a level within a map. Typically contains actors.

**Syntax:**
```
Begin Level NAME=<LevelName>
    <Actor Blocks>
End Level
```

**Attributes:**
- `NAME` (required): The name of the level (typically "PersistentLevel")

**Example:**
```
Begin Level NAME=PersistentLevel
    Begin Actor Class=StaticMeshActor Name=StaticMeshActor_0
    End Actor
End Level
```

### Actor Block

Represents an actor (object) in the level. Actors are the primary entities in Unreal Engine.

**Syntax:**
```
Begin Actor Class=<ClassName> Name=<ActorName> [Archetype=<ArchetypePath>] [ExportPath="<FullPath>"]
    <Properties>
    <Component Blocks>
End Actor
```

**Attributes:**
- `Class` (required): The C++ class name of the actor (e.g., `StaticMeshActor`, `PointLight`, `CameraActor`). May include full path like `/Script/Engine.StaticMeshActor`
- `Name` (required): Unique identifier for the actor within the level
- `Archetype` (optional): Path to a template object that provides default properties
- `ExportPath` (optional): Full object path for export (used in class default and asset exports)

**Common Properties:**
- `Location=(X=<float>,Y=<float>,Z=<float>)`: World position
- `Rotation=(Pitch=<int>,Yaw=<int>,Roll=<int>)`: Orientation (in Unreal rotation units, where 65536 = 360°)
- `DrawScale3D=(X=<float>,Y=<float>,Z=<float>)`: Non-uniform scale
- `DrawScale=<float>`: Uniform scale
- `Tag=<string>`: Actor tag for identification
- `bHidden=<bool>`: Visibility flag
- `bCollideActors=<bool>`: Collision flag

**Example:**
```
Begin Actor Class=StaticMeshActor Name=StaticMeshActor_0
    Location=(X=868.000000,Y=31.000000,Z=1.000000)
    Rotation=(Pitch=0.000000,Yaw=0.000000,Roll=0.000000)
    DrawScale3D=(X=2241.742760,Y=2241.742760,Z=2241.742760)
    StaticMesh=StaticMesh'ASC_Floor2.SM.Mesh.S_ASC_Floor2_SM_Stairs_Simple_01'
End Actor
```

### Component Block

Components are sub-objects attached to actors.

**Syntax:**
```
Begin Object Class=<ComponentClass> Name=<ComponentName> [Archetype=<ArchetypePath>] [ExportPath="<FullPath>"]
    <Properties>
    <Nested Components>
End Object
```

**Attributes:**
- `Class` (required): The component class name (may include full path like `/Script/Engine.StaticMeshComponent`)
- `Name` (required): Component identifier
- `Archetype` (optional): Template reference
- `ExportPath` (optional): Full object path for export (e.g., `"/Script/Engine.StaticMeshComponent'/Script/Engine.Default__StaticMeshActor:StaticMeshComponent0'"`)

**Example:**
```
Begin Object Class=/Script/Engine.StaticMeshComponent Name=StaticMeshComponent0 ExportPath="/Script/Engine.StaticMeshComponent'/Script/Engine.Default__StaticMeshActor:StaticMeshComponent0'"
    StaticMesh=StaticMesh'/Game/Meshes/MyMesh'
    RelativeLocation=(X=0.000000,Y=0.000000,Z=0.000000)
End Object
```

### Brush Block

Represents BSP (Binary Space Partitioning) geometry used in level design.

**Syntax:**
```
Begin Brush Name=<BrushName>
    <Polygon Definitions>
End Brush
```

**Polygon Syntax:**
```
Begin Polygon Flags=<Flags> Link=<LinkIndex>
    Vertex   <X> <Y> <Z>
    Vertex   <X> <Y> <Z>
    Vertex   <X> <Y> <Z>
    Pan      U=<float> V=<float>
    Texture  <TextureName>
End Polygon
```

## Property Syntax

### Basic Property Format

Properties follow the pattern: `<PropertyName>=<Value>`

### Default Value Handling

**IMPORTANT:** T3D format uses default value omission for efficiency:

- **Export Behavior**: Properties with values matching their class defaults are **omitted** from exports
- **Import Behavior**: Omitted properties resolve to their class default values
- **Best Practice**: When creating/editing T3D files, omit properties that match defaults

This means:
- A T3D file only contains properties that differ from defaults
- Parsers must resolve omitted properties to their class defaults
- Specifying default values explicitly is valid but unnecessary

**Example:**
```
# If Actor.bHidden defaults to False:
# Export with bHidden=False → property is OMITTED
# Export with bHidden=True → property is INCLUDED: bHidden=True

# When importing:
# If bHidden is omitted → resolves to False (default)
# If bHidden=True is specified → resolves to True
# If bHidden=False is specified → resolves to False (valid but redundant)
```

### Value Types

#### Vectors
```
Location=(X=100.0,Y=200.0,Z=300.0)
DrawScale3D=(X=1.0,Y=1.0,Z=1.0)
```

#### Rotators
```
Rotation=(Pitch=0,Yaw=16384,Roll=0)
```
**Note:** Unreal Engine uses integer rotation units where 65536 = 360 degrees. To convert:
- Degrees to Unreal units: `value * 65536 / 360`
- Unreal units to degrees: `value * 360 / 65536`

#### Booleans

Booleans use `True` or `False` (case-sensitive):

```
bHidden=True
bCollideActors=False
bIsIntermediateNode=False
bAllowDeletion=False
```

**Note:** Boolean values are always capitalized (`True`/`False`), not lowercase.

#### Strings

Strings can be quoted or unquoted:

```
Tag=MyActorTag
Name="MyActorName"
NodeComment="This node is disabled and will not be called.\nDrag off pins to build functionality."
```

**Note:** Strings containing special characters, spaces, or newlines should be quoted. Newlines in strings are represented as `\n`.

#### Object References

Object references use the format: `'PackagePath.ObjectName'` or `Class'PackagePath.ObjectName'`

**Full Path:**
```
StaticMesh=StaticMesh'/Game/Meshes/MyMesh.MyMesh'
```

**Short Path (when using PPF_ExportsNotFullyQualified flag):**
```
StaticMesh=StaticMesh'MyMesh'
```

**Blueprint References:**
```
Blueprint=Blueprint'/Game/Blueprints/MyBlueprint.MyBlueprint'
```

#### Arrays

Arrays are represented as space-separated values or multi-line entries:

```
Tags=(Tag1,Tag2,Tag3)
```

Or:
```
Tags=(
    Tag1
    Tag2
    Tag3
)
```

**Empty Arrays:**
```
OnTakeAnyDamage=()
CustomPrimitiveData=(Data=)
```

#### Structs

Structs use nested parentheses:

```
RelativeLocation=(X=0.0,Y=0.0,Z=0.0)
RelativeRotation=(Pitch=0,Yaw=0,Roll=0)
```

**Complex Nested Structs:**
```
BodyInstance=(PositionSolverIterationCount=8,VelocitySolverIterationCount=2,ObjectType=ECC_WorldStatic,CollisionProfileName="BlockAll",CollisionResponses=(ResponseArray=((Channel="Projectile"),(Channel="Effectable"))),WalkableSlopeOverride=(WalkableSlopeBehavior=WalkableSlope_Default,WalkableSlopeAngle=0.000000))
```

#### Enums

Enum values are represented as unquoted identifiers:

```
DepthPriorityGroup=SDPG_World
IndirectLightingCacheQuality=ILCQ_Point
CanCharacterStepUpOn=ECB_Yes
Mobility=Static
```

#### Pin Connections (Blueprint Graphs)

Pin connections use the `LinkedTo` property:

```
CustomProperties Pin (PinId=...,PinName="then",...,LinkedTo=(K2Node_CallFunction_0 13D6539744ED8B7D0105FB89CB2BD205,),...)
```

The `LinkedTo` value is a comma-separated list of `(NodeName PinId)` pairs.

#### Default Values

**Critical Behavior:** T3D format uses default value omission for optimization:

**When Exporting:**
- Properties with values identical to their class defaults are **omitted** from the export
- Only properties that differ from defaults are included
- This significantly reduces file size and focuses on actual differences

**When Importing:**
- Properties not specified in the T3D file will use their class default values
- Specifying a property value that matches the default is **valid but optional**
- Best practice: Omit properties that match defaults to keep files clean

**Example:**
If an Actor's default `bHidden` is `False`, and an instance also has `bHidden=False`:
- **Export:** The `bHidden` property is omitted (not written to T3D)
- **Import:** If `bHidden` is not specified, it defaults to `False`
- **Import:** If `bHidden=False` is specified, it's valid but redundant

**Pin Default Values (Blueprint-specific):**

Blueprint node pins can have both `DefaultValue` and `AutogeneratedDefaultValue`:

```
DefaultValue="0.0",AutogeneratedDefaultValue="0.0"
DefaultValue="God",AutogeneratedDefaultValue=""
AutogeneratedDefaultValue="None"
```

**Note:** The default value omission rule applies to all properties, not just Blueprint pins. When parsing T3D files, AI agents should assume omitted properties use their class defaults.

## Common Actor Classes

### Static Mesh Actor
```
Begin Actor Class=StaticMeshActor Name=<Name>
    Location=(X=0,Y=0,Z=0)
    Rotation=(Pitch=0,Yaw=0,Roll=0)
    StaticMesh=<StaticMeshReference>
End Actor
```

### Point Light
```
Begin Actor Class=PointLight Name=<Name>
    Location=(X=0,Y=0,Z=0)
    LightComponent=PointLightComponent'<ComponentName>'
End Actor
```

### Camera Actor
```
Begin Actor Class=CameraActor Name=<Name>
    Location=(X=0,Y=0,Z=0)
    Rotation=(Pitch=0,Yaw=0,Roll=0)
End Actor
```

### Blueprint Actor
```
Begin Actor Class=<BlueprintGeneratedClass> Name=<Name>
    Location=(X=0,Y=0,Z=0)
    <Blueprint-specific properties>
End Actor
```

## Export Flags

When exporting T3D files programmatically, common export flags include:

- `PPF_Copy`: Export as a copy
- `PPF_ExportsNotFullyQualified`: Use short object paths instead of full paths
- `PPF_Localized`: Include localized text
- `PPF_DeepCompareInstances`: Deep comparison of instances

## Blueprint Export Format

When exporting Blueprint assets to T3D, the format includes:

1. **Blueprint Asset Definition**: The Blueprint asset itself (not the generated class)
2. **Graph Definitions**: All Blueprint graphs (EventGraph, UserConstructionScript, custom functions, ExecuteUbergraph, MERGED variants)
3. **Graph Nodes**: All nodes within each graph
4. **Node Properties**: Pin connections, positions, and node-specific properties
5. **Blueprint Metadata**: Parent class, generated class reference, Blueprint GUID, category sorting, last edited documents, etc.

**Example Blueprint Export Structure:**
```
Begin Object Class=/Script/Engine.Blueprint Name="<BlueprintName>" ExportPath="/Script/Engine.Blueprint'<FullPath>'"
    Begin Object Class=/Script/Engine.EdGraph Name="EventGraph" ExportPath="<GraphPath>"
        Begin Object Class=/Script/BlueprintGraph.K2Node_Event Name="<NodeName>" ExportPath="<NodePath>"
            <Node Properties>
            CustomProperties Pin (PinId=<Guid>,PinName="<PinName>",LinkedTo=(...),...)
        End Object
    End Object
    ParentClass="/Script/Engine.BlueprintGeneratedClass'<ParentClassPath>'"
    GeneratedClass="/Script/Engine.BlueprintGeneratedClass'<GeneratedClassPath>'"
    BlueprintGuid=<GUID>
    CategorySorting(0)="<Category1>"
    LastEditedDocuments(0)=(EditedObjectPath="<Path>",SavedZoomAmount=1.000000)
End Object
```

### Blueprint Graph Types

**Standard Graphs:**
- `EventGraph`: Main event graph
- `UserConstructionScript`: Construction script
- `ExecuteUbergraph_<BlueprintName>`: Generated uber graph for compiled Blueprint code
- `UserConstructionScript_MERGED`: Merged construction script
- Custom function graphs: Named after the function

**Empty Graphs:**
Graphs may be empty (no nodes), containing only:
```
Begin Object Name="EventGraph" ExportPath="..."
    Schema="/Script/CoreUObject.Class'/Script/BlueprintGraph.EdGraphSchema_K2'"
    bAllowDeletion=False
    GraphGuid=<GUID>
End Object
```

### Blueprint Node Properties

**Common Node Properties:**
- `NodePosX=<int>`, `NodePosY=<int>`: Node position in graph
- `NodeGuid=<GUID>`: Unique identifier for the node
- `bIsIntermediateNode=True/False`: Whether node is intermediate (compiled code)
- `EnabledState=Enabled/Disabled`: Node enabled state
- `NodeComment="<text>"`: Node comment text
- `bCommentBubblePinned=True/False`: Comment bubble visibility
- `CustomGeneratedFunctionName="<name>"`: For function entry nodes

**Pin Properties:**
- `PinId=<GUID>`: Unique pin identifier
- `PinName="<name>"`: Pin name
- `Direction="EGPD_Input"/"EGPD_Output"`: Pin direction
- `PinType.PinCategory="<category>"`: Pin type category (exec, object, real, string, bool, etc.)
- `PinType.PinSubCategory="<subcategory>"`: Subcategory (float, double, etc.)
- `PinType.PinSubCategoryObject="<class>"`: Object class for object pins
- `DefaultValue="<value>"`: Default value
- `AutogeneratedDefaultValue="<value>"`: Auto-generated default
- `LinkedTo=(<NodeName> <PinId>,...)`: Connections to other pins
- `PinToolTip="<text>"`: Tooltip text
- `bHidden=True/False`: Pin visibility
- `PersistentGuid=<GUID>`: Persistent identifier

**Note:** Blueprint exports use `Begin Object` with `Class=/Script/Engine.Blueprint`, not `Begin Map`. The Blueprint asset structure includes graph definitions and node data, not actor instances.

## Parsing Guidelines

### For AI Agents

When parsing T3D files:

1. **Hierarchical Parsing**: Use a stack-based parser to track nested `Begin`/`End` blocks
2. **Property Extraction**: Extract properties as key-value pairs, handling nested structures
3. **Default Value Resolution**: **CRITICAL** - Resolve omitted properties to their class defaults. Properties not present in the T3D file use their class default values.
4. **Object Reference Resolution**: Resolve object references to determine asset dependencies
5. **Type Inference**: Infer property types from context and value format
6. **Error Handling**: Handle malformed blocks, missing End tags, and invalid property values

**Default Value Resolution Process:**
- Load the class default object for the exported class
- For each property in the T3D file, use the specified value
- For each property NOT in the T3D file, use the class default value
- This ensures complete property sets even when properties are omitted

### Common Parsing Patterns

#### Extract Actor Location
```
Location=(X=100.0,Y=200.0,Z=300.0)
→ Parse as: {X: 100.0, Y: 200.0, Z: 300.0}
```

#### Extract Object Reference
```
StaticMesh=StaticMesh'/Game/Meshes/MyMesh.MyMesh'
→ Parse as: PackagePath='/Game/Meshes/MyMesh', ObjectName='MyMesh', Class='StaticMesh'
```

#### Extract Rotation (convert to degrees)
```
Rotation=(Pitch=16384,Yaw=32768,Roll=0)
→ Convert: Pitch=90°, Yaw=180°, Roll=0°
```

## Additional Property Formats

### Indexed Properties

Some properties use array-like indexing:

```
CategorySorting(0)="Default"
CategorySorting(1)=""
CategorySorting(2)="Event Graph"
LastEditedDocuments(0)=(EditedObjectPath="<Path>",SavedZoomAmount=1.000000)
LastEditedDocuments(1)=(EditedObjectPath="<Path>",SavedViewOffset=(X=-447.373199,Y=-460.246704),SavedZoomAmount=0.675000)
Nodes(0)="/Script/BlueprintGraph.K2Node_Event'K2Node_Event_0'"
Nodes(1)="/Script/BlueprintGraph.K2Node_CallFunction'K2Node_CallFunction_1'"
```

### Object Reference Formats

**Full Object Path:**
```
ParentClass="/Script/Engine.BlueprintGeneratedClass'/Game/Character/Sol/Blueprint/BP_Sol.BP_Sol_C'"
```

**Default Object Reference:**
```
DefaultObject="/Script/Engine.Default__KismetSystemLibrary"
```

**Component Reference:**
```
RootComponent="/Script/Engine.StaticMeshComponent'StaticMeshComponent0'"
```

### Function References

Function references in Blueprint nodes:

```
FunctionReference=(MemberParent="/Script/CoreUObject.Class'/Script/Engine.Actor'",MemberName="ReceiveBeginPlay")
FunctionReference=(MemberParent="/Script/Engine.BlueprintGeneratedClass'/Game/Common/Blueprints/BP_Basic.BP_Basic_C'",MemberName="ReceiveBeginPlay",MemberGuid=5A2BCA6E4D01881C4E30B3AFC8865B7D)
FunctionReference=(MemberName="ExecuteUbergraph_BP_Sol",bSelfContext=True)
```

### Variable References

Variable references in Blueprint nodes:

```
VariableReference=(MemberName="FX_Persist_Glow",bSelfContext=True)
```

## Default Value Resolution

**CRITICAL FOR PARSING:** T3D files use default value omission. This is one of the most important aspects of the format:

### Export Behavior
- Properties with values identical to class defaults are **omitted** from exports
- Only non-default values are written to T3D files
- This optimization reduces file size and focuses on actual differences

### Import/Parsing Behavior
- **Omitted properties MUST be resolved to class defaults**
- To get complete property sets, parsers must:
  1. Load the class default object for the exported class
  2. Merge T3D properties with class defaults
  3. Use T3D values where specified, defaults where omitted

### Example

**Class Default (Actor):**
```
bHidden=False (default)
bCanBeDamaged=True (default)
Location=(X=0,Y=0,Z=0) (default)
```

**T3D Export (Actor instance):**
```
Begin Actor Class=StaticMeshActor Name=MyActor
    Location=(X=100,Y=200,Z=50)
    bCanBeDamaged=False
End Actor
```

**Parsed Result:**
- `Location=(X=100,Y=200,Z=50)` - from T3D (non-default)
- `bCanBeDamaged=False` - from T3D (non-default)
- `bHidden=False` - from class default (omitted in T3D)

**Note:** Specifying default values explicitly in T3D files is valid but unnecessary. Best practice is to omit them.

## Limitations and Notes

1. **Import Deprecation**: Direct T3D import was deprecated in Unreal Engine 4.13+ due to stability issues. T3D files are primarily used for export and analysis.

2. **Format Variations**: The exact format may vary slightly between Unreal Engine versions. Always validate against the engine version being used.

3. **Binary Data**: T3D files are text-only. Binary data (textures, meshes) are referenced but not embedded.

4. **Incomplete Exports**: Some complex objects may not export all properties. Always verify critical properties are present. **Remember: Omitted properties are intentional (default values), not missing data.**

5. **Line Endings**: T3D files typically use Windows line endings (`\r\n`), but Unix line endings (`\n`) are also valid.

6. **Empty Graphs**: Blueprint graphs may be empty (containing only Schema and GraphGuid) if they have no nodes.

7. **MERGED Graphs**: Blueprint exports may include `_MERGED` variants of graphs (e.g., `UserConstructionScript_MERGED`) which represent compiled/merged versions.

8. **Intermediate Nodes**: Some nodes are marked `bIsIntermediateNode=True`, indicating they are part of compiled Blueprint code rather than user-editable nodes.

9. **Default Value Omission**: This is the most critical aspect for parsers - omitted properties are not errors, they indicate default values should be used.

## Validation Checklist

When validating a T3D file:

- [ ] File starts with `Begin Map` (for level exports) OR `Begin Object` (for asset/class exports)
- [ ] File ends with matching `End Map` or `End Object`
- [ ] All `Begin` blocks have matching `End` blocks
- [ ] Object/Actor blocks have `Class` and `Name` attributes
- [ ] Property values match expected types
- [ ] Object references use valid syntax (e.g., `Class'Path.Object'`)
- [ ] No unclosed parentheses in property values
- [ ] Rotation values are within valid range (0-65535)
- [ ] ExportPath attributes (if present) use valid object reference format
- [ ] **Default values**: Understand that omitted properties are intentional (use class defaults), not missing data

## Example: Complete T3D File

**Note:** This example shows properties that would typically be omitted if they match defaults. In actual exports, default values are omitted.

```
Begin Map Name=TestMap
    Begin Level NAME=PersistentLevel
        Begin Actor Class=StaticMeshActor Name=StaticMeshActor_0
            Location=(X=100.000000,Y=200.000000,Z=50.000000)
            Rotation=(Pitch=0.000000,Yaw=16384.000000,Roll=0.000000)
            DrawScale3D=(X=2.000000,Y=2.000000,Z=2.000000)
            StaticMesh=StaticMesh'/Game/Meshes/TestMesh.TestMesh'
            Tag=TestActor
            bHidden=True
            Begin Object Class=StaticMeshComponent Name=StaticMeshComponent0
                StaticMesh=StaticMesh'/Game/Meshes/TestMesh.TestMesh'
                RelativeLocation=(X=10.000000,Y=0.000000,Z=0.000000)
            End Object
        End Actor
    End Level
End Map
```

**Default Value Resolution Example:**

For the above T3D file, assuming `StaticMeshActor` class defaults:
- `Location=(X=100,Y=200,Z=50)` - **Included** (non-default position)
- `Rotation=(Pitch=0,Yaw=16384,Roll=0)` - **Included** (non-default rotation, Yaw=90°)
- `DrawScale3D=(X=2,Y=2,Z=2)` - **Included** (non-default scale)
- `StaticMesh=...` - **Included** (required, no default)
- `Tag=TestActor` - **Included** (non-default tag)
- `bHidden=True` - **Included** (non-default, default is False)
- `bCollideActors` - **OMITTED** (matches default True)
- `RelativeLocation=(X=10,Y=0,Z=0)` - **Included** (X=10 is non-default)
- `RelativeRotation` - **OMITTED** (matches default Pitch=0,Yaw=0,Roll=0)
- `RelativeScale3D` - **OMITTED** (matches default X=1,Y=1,Z=1)

**When Parsing:**
- Properties in T3D → use T3D values
- Properties NOT in T3D → use class default values
- Result: Complete property set with defaults filled in

## References

- Unreal Engine Documentation: Export/Import functionality
- Unreal Engine Source Code: `UExporter` class and T3D exporter implementations
- MIME Type: `application/vnd.unreal.t3d` (as defined in UnrealMCPServer plugin)

## Version Information

**Document Version:** 1.0  
**Last Updated:** 2024  
**Unreal Engine Compatibility:** UE 4.x, UE 5.x  
**Format Status:** Export supported, Import deprecated (UE 4.13+)
