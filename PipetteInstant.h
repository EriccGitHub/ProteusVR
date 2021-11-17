#pragma once

#include "CoreMinimal.h"
#include "Pipette.h"
#include "PipetteInstant.generated.h"

UCLASS(Blueprintable, ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class PROTEUS_API UPipetteInstant : public UPipette
{
	GENERATED_BODY()

protected:
	virtual void DoInteraction() override;
};