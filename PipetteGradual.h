#pragma once

#include "CoreMinimal.h"
#include "Pipette.h"
#include "PipetteGradual.generated.h"

UCLASS(Blueprintable, ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class PROTEUS_API UPipetteGradual : public UPipette
{
	GENERATED_BODY()

protected:
	virtual void DoInteraction() override;

private:
	void ExchangeLiquidGradually(float startingMl, ULiquidContainerComp* otherLiquidContainer);
};
