#pragma once

#include "CoreMinimal.h"
#include "PipetteInstant.h"
#include "PipetteNumeric.generated.h"

UCLASS(Blueprintable, ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class PROTEUS_API UPipetteNumeric : public UPipetteInstant
{
	GENERATED_BODY()
	
public :
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	UPROPERTY(EditAnywhere)
		AActor* emptyPlunger;
	UPROPERTY(EditAnywhere)
		AActor* fillPlunger;
	void UpdatePlunger();
	void UpdateText();
	float mlOnScreen = 0;
	UTextRenderComponent* text;
	UStaticMeshComponent* plunger;
};
