#include "UMCP_T3DFallbackFactory.h"
#include "UObject/UnrealType.h"
#include "Factories/Factory.h"
#include "Editor/UnrealEd/Public/UnrealEd.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/Package.h"
#include "UnrealMCPServerModule.h"

UMCP_T3DFallbackFactory::UMCP_T3DFallbackFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Set low priority so this factory is only used as a fallback
	ImportPriority = -1000;
	
	// Support T3D file format via the Formats array (parent class uses this for FactoryCanImport)
	Formats.Add(TEXT("t3d;Text 3D"));
	
	bCreateNew = false;
	bEditorImport = true;
	bText = true;
	
	// Initialize SupportedClass to this factory's own class (disabled state)
	// This ensures SupportedClass is never nullptr
	SupportedClass = StaticClass();
}

void UMCP_T3DFallbackFactory::SetSupportedClass(UClass* InSupportedClass)
{
	// Get the default factory instance and modify its SupportedClass member directly
	UMCP_T3DFallbackFactory* DefaultFactory = StaticClass()->GetDefaultObject<UMCP_T3DFallbackFactory>();
	if (DefaultFactory)
	{
		// If nullptr is passed, set to factory's own class (disabled state)
		// This ensures SupportedClass is never nullptr
		if (InSupportedClass == nullptr)
		{
			DefaultFactory->SupportedClass = StaticClass();
		}
		else
		{
			DefaultFactory->SupportedClass = InSupportedClass;
		}
	}
}

UObject* UMCP_T3DFallbackFactory::FactoryCreateFile(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, const FString& Filename, const TCHAR* Parms, FFeedbackContext* Warn, bool& bOutOperationCanceled)
{
	// Read the T3D file content
	FString T3DContent;
	if (!FFileHelper::LoadFileToString(T3DContent, *Filename))
	{
		if (Warn)
		{
			Warn->Logf(ELogVerbosity::Error, TEXT("Failed to read T3D file: %s"), *Filename);
		}
		return nullptr;
	}
	
	// Import the T3D properties into the object
	const TCHAR* T3DBuffer = *T3DContent;
	FImportObjectParams ImportParams;
	ImportParams.SourceText = T3DBuffer;
	ImportParams.DestData = (uint8*)InParent;
	ImportParams.ObjectStruct = InClass; // UClass inherits from UStruct
	ImportParams.SubobjectRoot = InParent;
	ImportParams.SubobjectOuter = InParent;
	ImportParams.Warn = Warn;
	// Depth defaults to 0 (appropriate for top-level import)
	// LineNumber defaults to INDEX_NONE (appropriate when not tracking line numbers)
	// InInstanceGraph defaults to nullptr (appropriate for simple imports)
	// ObjectRemapper defaults to nullptr (appropriate when no remapping needed)
	// bShouldCallEditChange defaults to true (appropriate for editor imports)
	
	// Import the object properties from the T3D file
	ImportObjectProperties(ImportParams);

	auto pCreatedObject = FindObject<UObject>(InParent, *InName.ToString());
	
	return pCreatedObject;
}

