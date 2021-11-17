#include "PipetteGradual.h"

void UPipetteGradual::DoInteraction()
{
	Super::DoInteraction();
	if (filled)
	{
		if (interactedLiquidContainer->GetCurrentMl() <= 0) //other container empty
		{
			ExchangeLiquidGradually(0, interactedLiquidContainer);
			interactedLiquidContainer->ChangeLiquidColor(liquidContainer->currentLiquidColor);
		}
		else
		{
			if (interactedLiquidContainer->CanAcceptLiquidFrom(ingredientComponent))
			{
				ExchangeLiquidGradually(0, interactedLiquidContainer);
				if (ingredientComponent)
					if (interactedLiquidContainer->ingredientComp)
						ingredientComponent->TransferAllIngredientsToByAmount(interactedLiquidContainer->ingredientComp, 2);
			}
		}
	}
	else
	{
		//TODO, as of the time of writing this code is only used for one actor in the 
		//lake and it doesn't have to be empty at any time so left this part empty.
	}
}

void UPipetteGradual::ExchangeLiquidGradually(float startingMl, ULiquidContainerComp* otherLiquidContainer)
{
	if (!interacting)
		return;

	float step = 2;
	float stepSpeed = 0.1;
	if (startingMl >= outTransferRate)
		return;

	liquidContainer->ChangeMl(liquidContainer->GetCurrentMl() - step);
	otherLiquidContainer->ChangeMl(otherLiquidContainer->GetCurrentMl() + step);
	FTimerDelegate timerDelegate;
	FTimerHandle timerHandle;
	timerDelegate.BindUObject(this, &UPipetteGradual::ExchangeLiquidGradually, startingMl + step, otherLiquidContainer);
	GetWorld()->GetTimerManager().SetTimer(timerHandle, timerDelegate, stepSpeed, false);
}