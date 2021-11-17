#include "PipetteInstant.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInstanceDynamic.h"

void UPipetteInstant::DoInteraction()
{
	if (!interactedLiquidContainer)
		return;

	Super::DoInteraction();
	if (filled)
	{
		if (interactedLiquidContainer->CanAcceptLiquidFrom(ingredientComponent))
		{
			interactedLiquidContainer->AddMl(outTransferRate);
			if (ingredientComponent)
				ingredientComponent->TransferAllIngredientsToByAmount(interactedLiquidContainer->ingredientComp, outTransferRate);

			EmptyPipette();
		}
	}
	else
	{
		if (interactedLiquidContainer->GetCurrentMl() >= liquidContainer->maxMl) //other container isnt empty
		{
			FillPipette();
			interactedLiquidContainer->ChangeMl(interactedLiquidContainer->GetCurrentMl() - inTransferRate);
		}
		else
		{
			//if other container is close to empty, should fill by X amount
		}
	}
}