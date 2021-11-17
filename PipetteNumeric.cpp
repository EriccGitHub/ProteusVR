#include "PipetteNumeric.h"

void UPipetteNumeric::BeginPlay()
{
	Super::BeginPlay();
	PrimaryComponentTick.bCanEverTick = true;
	TArray<UActorComponent*> staticMeshes = GetOwner()->GetComponentsByTag(UStaticMeshComponent::StaticClass(), "FINGER");
	if (staticMeshes.Num() > 0)
		plunger = Cast<UStaticMeshComponent>(staticMeshes[0]);

	if (!IsRunningDedicatedServer())
		text = Cast<UTextRenderComponent>(GetOwner()->GetComponentByClass(UTextRenderComponent::StaticClass()));
}

void UPipetteNumeric::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (mlOnScreen != liquidContainer->GetCurrentMl())
	{
		float updateSpeed = 0.8f;
		if (abs(mlOnScreen - liquidContainer->GetCurrentMl()) <= updateSpeed)
			mlOnScreen = liquidContainer->GetCurrentMl();
		else if (mlOnScreen > liquidContainer->GetCurrentMl())
			mlOnScreen -= updateSpeed;
		else
			mlOnScreen += updateSpeed;

		UpdateText();
		UpdatePlunger();
	}
}

void UPipetteNumeric::UpdatePlunger()
{
	if (!plunger || !liquidContainer || !fillPlunger)
		return;

	bool isFilling = mlOnScreen < liquidContainer->GetCurrentMl();
	FVector newPostion;
	float alpha = mlOnScreen * 1 / liquidContainer->maxMl;
	if (isFilling)
		newPostion = FMath::Lerp(emptyPlunger->GetActorLocation(), fillPlunger->GetActorLocation(), alpha);
	else
		newPostion = FMath::Lerp(fillPlunger->GetActorLocation(), emptyPlunger->GetActorLocation(), 1 - alpha);

	plunger->SetWorldLocation(newPostion);
}

void UPipetteNumeric::UpdateText()
{
	if (!text)
		return;

	text->SetText(FText::FromString(*FString::SanitizeFloat(mlOnScreen)));
}