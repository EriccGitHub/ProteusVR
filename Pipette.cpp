#include "Pipette.h"
#include "GameFramework/Actor.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "CollisionQueryParams.h"
#include "Engine/World.h"
#include "Proteus/Components/Liquid/LiquidContainerComp.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Runtime/Engine/Classes/Kismet/GameplayStatics.h"
#include "Proteus/GeneralProject/Pawn/PawnProteusBase.h"

UPipette::UPipette()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	this->SetIsReplicatedByDefault(true);
}

void UPipette::BeginPlay()
{
	Super::BeginPlay();
	liquidContainer = Cast<ULiquidContainerComp>(GetOwner()->GetComponentByClass(ULiquidContainerComp::StaticClass()));
	liquidContainer->OnChangedMl.AddDynamic(this, &UPipette::ResetData);
	pipetteLogging = Utility::GetComponentUtility<UPipetteLoggingBase>(GetOwner());
	ingredientComponent = Utility::GetComponentUtility<UIngredientsContainerComponent>(GetOwner());
	if (liquidContainer->startingMl > 0)
		filled = true;
}

void UPipette::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

void UPipette::InteractStart_Implementation()
{
	AActor* actorBelowPipette = GetContainerBelow();
	if (!actorBelowPipette)
		return;

	interacting = true;
	ULiquidContainerComp* otherLiquidContainer = Cast<ULiquidContainerComp>(actorBelowPipette->GetComponentByClass(ULiquidContainerComp::StaticClass()));
	if (otherLiquidContainer)
	{
		if (otherLiquidContainer->hasCap)
			return;

		if (pipetteLogging && ingredientComponent)
			pipetteLogging->LookForUpdateJournal(otherLiquidContainer, ingredientComponent->ingredientsAdded, outTransferRate);

		interactedLiquidContainer = otherLiquidContainer;
		if (!otherLiquidContainer->GetOwner()->GetOwner())
		{
			AActor* owner = GetOwner()->GetOwner();
			APawnProteusBase::Delegate ChangingActorOwnerCallback;
			ChangingActorOwnerCallback.AddUObject(this, &UPipette::DoInteraction);
			Cast<APawnProteusBase>(owner)->ChangeActorOwner(otherLiquidContainer->GetOwner(), owner, ChangingActorOwnerCallback);
		}
		else
		{
			DoInteraction();
		}
	}
}

void UPipette::InteractStop_Implementation()
{
	interacting = false;
	if (interactedLiquidContainer)
	{
		AActor* owner = GetOwner()->GetOwner();
		APawnProteusBase::Delegate nullDelegate;
		Cast<APawnProteusBase>(owner)->ChangeActorOwner(interactedLiquidContainer->GetOwner(), nullptr, nullDelegate);
	}
	interactedLiquidContainer = nullptr;
}

void UPipette::DoInteraction()
{
	if (!interactedLiquidContainer)
		return;

	if (filled)
	{
		FLinearColor currentLiquidColor;
		liquidContainer->liquidMaterial->GetVectorParameterValue(TEXT("Liquid_Color"), currentLiquidColor);
		if (interactedLiquidContainer->GetCurrentMl() <= 0) //other container empty
		{
			interactedLiquidContainer->ChangeLiquidColor(liquidContainer->currentLiquidColor);
		}
		else
		{
			FLinearColor otherLiquidColor;
			interactedLiquidContainer->liquidMaterial->GetVectorParameterValue(TEXT("Liquid_Color"), otherLiquidColor);
			FLinearColor colorToApply = otherLiquidColor + outTransferRate / liquidContainer->maxMl * (currentLiquidColor - otherLiquidColor);
			float factor = outTransferRate / (interactedLiquidContainer->GetCurrentMl() + outTransferRate);
			//interactedLiquidContainer->MixLiquidColorWith(colorToApply, factor);
		}
	}
	else
	{
		if (ingredientComponent)
		{
			FLinearColor otherLiquidColor;
			interactedLiquidContainer->liquidMaterial->GetVectorParameterValue(TEXT("Liquid_Color"), otherLiquidColor);
			liquidContainer->ChangeLiquidColor(otherLiquidColor);
			UIngredientsContainerComponent* otherIngredientComp = Utility::GetComponentUtility<UIngredientsContainerComponent>(interactedLiquidContainer->GetOwner());
			if (otherIngredientComp)
				otherIngredientComp->TransferAllIngredientsTo(liquidContainer, inTransferRate);
		}
	}
}

AActor* UPipette::GetContainerBelow()
{
	FHitResult hitResult;
	FVector traceStart = pipetteSocket->GetActorLocation();
	FVector traceEnd = FVector(traceStart.X, traceStart.Y, traceStart.Z - 2.5f);
	FCollisionQueryParams params = FCollisionQueryParams("PipetteTrace", false, GetOwner());
	if (GetWorld()->LineTraceSingleByChannel(hitResult, traceStart, traceEnd, ECC_Visibility, params))
		return hitResult.GetActor();

	return nullptr;
}

void UPipette::FillPipette()
{
	if (filled)
		EmptyPipette();


	liquidContainer->ChangeMl(inTransferRate);
	if (GetOwner()->GetOwner() || IsRunningDedicatedServer())
		S_SetFilled(true);
}

void UPipette::EmptyPipette()
{
	if (!filled)
		return;

	liquidContainer->ChangeMl(liquidContainer->GetCurrentMl() - outTransferRate);
	ResetData();
}

void UPipette::ResetData()
{
	if (liquidContainer->GetCurrentMl() <= 0)
	{
		if (GetOwner()->HasNetOwner() || IsRunningDedicatedServer())
		{

			S_SetFilled(false);
			if (ingredientComponent)
				ingredientComponent->S_ClearAllIngredientsAdded();
		}
	}
}

void UPipette::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UPipette, filled);
}

void UPipette::S_SetFilled_Implementation(bool newValue)
{
	filled = newValue;
}