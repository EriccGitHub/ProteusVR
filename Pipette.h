#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Proteus/Components/Liquid/LiquidContainerComp.h"
#include "Components/TextRenderComponent.h"
#include "Math/TransformNonVectorized.h"
#include "Proteus/Objects/Data/Ingredient/PhData.h"
#include "Proteus/GeneralProject/Utility.h"
#include "Proteus/Interface/Interact.h"
#include "PipetteLoggingBase.h"
#include "Pipette.generated.h"

UCLASS(Blueprintable, ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class PROTEUS_API UPipette : public UActorComponent, public IInteract
{
	GENERATED_BODY()

public:	
	UPipette();
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	void EmptyPipette();
	virtual void FillPipette();
	UFUNCTION()
		virtual void ResetData();
	virtual void DoInteraction();
	AActor* GetContainerBelow();
	bool interacting = false;
	ULiquidContainerComp* interactedLiquidContainer;
	UPipetteLoggingBase* pipetteLogging;
	UIngredientsContainerComponent* ingredientComponent;
	UPROPERTY(Replicated)
		bool filled = false;
	UPROPERTY(BlueprintReadOnly)
		ULiquidContainerComp* liquidContainer;
	UPROPERTY(EditAnywhere)
		float inTransferRate = 5;
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
		float outTransferRate = 5;
	UPROPERTY(EditAnywhere)
		AActor* pipetteSocket;

private:
	UFUNCTION(Server, Reliable)
		void S_SetFilled(bool newValue);

public:

	void InteractStart_Implementation() override;
	void InteractStop_Implementation() override;
};