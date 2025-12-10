#pragma once

#include "CoreMinimal.h"
#include "Factories/Factory.h"
#include "UMCP_T3DFallbackFactory.generated.h"

/**
 * Fallback factory for importing T3D files with any class.
 * This factory only supports classes when explicitly enabled via SetSupportedClass().
 * It has a low priority to ensure other factories are preferred when available.
 * The factory's SupportedClass member is temporarily modified to control which classes it supports.
 */
UCLASS()
class UNREALMCPSERVER_API UMCP_T3DFallbackFactory : public UFactory
{
	GENERATED_BODY()

public:
	UMCP_T3DFallbackFactory(const FObjectInitializer& ObjectInitializer);

	// UFactory interface
	virtual UObject* FactoryCreateFile(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, const FString& Filename, const TCHAR* Parms, FFeedbackContext* Warn, bool& bOutOperationCanceled) override;

	/**
	 * Set the temporary supported class for this factory by modifying the parent's SupportedClass member.
	 * When set to nullptr or the factory's own class, the factory is disabled.
	 * The class should remain set until the import operation completes.
	 * @param SupportedClass The class to temporarily support, or nullptr to disable
	 */
	static void SetSupportedClass(UClass* SupportedClass);
};

