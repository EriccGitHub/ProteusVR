#include "LiquidContainerComp.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Math/BoxSphereBounds.h"
#include "DrawDebugHelpers.h"
#include <Runtime\Engine\Public\Net\UnrealNetwork.h>
#include "Proteus/GeneralProject/Pawn/PawnProteusBase.h"
#include "Proteus/Actors/GenericObjects/GenericHandObjectBase.h"
#include <Proteus/Objects/Data/Ingredient/LiquidIngredientData.h>
#include "Math/UnrealMathUtility.h"

#pragma region inherited
ULiquidContainerComp::ULiquidContainerComp()
{
	PrimaryComponentTick.bCanEverTick = true;
	this->SetIsReplicatedByDefault(true);
}

void ULiquidContainerComp::BeginPlay()
{
	Super::BeginPlay();
	InitLiquidMaterial();
	InitPouring();
	genericHandOwner = Cast<AGenericHandObjectBase>(GetOwner());
	ingredientComp = Utility::GetComponentUtility<UIngredientsContainerComponent>(GetOwner());
	drippingAudio = Utility::GetComponentUtility<UAudioComponent>(GetOwner());
	TArray<UActorComponent*> boxColliders;
	GetOwner()->GetComponents(boxColliders);
	for (int i = 0; i < boxColliders.Num(); ++i)
	{
		UBoxComponent* boxColliderCast = Cast<UBoxComponent>(boxColliders[i]);
		if (boxColliderCast)
		{
			//												ECollisionChannel::LiquidPouring
			if (boxColliderCast->GetCollisionResponseToChannel(ECollisionChannel::ECC_GameTraceChannel1) == ECollisionResponse::ECR_Block)
			{
				boxCollider = boxColliderCast;
				break;
			}
		}
	}
	if (liquidMaterial)
	{
		if (!IsRunningDedicatedServer())
		{
			if (currentLiquidColor == FLinearColor(0, 0, 0, 0))//Making sure it's not already changed by the server for late joining players
			{
				if (ingredientComp)
				{
					for (int i = 0; i < ingredientComp->startingIngredients.Num(); ++i)
					{
						if (i == 0)
							ChangeLiquidColor(ingredientComp->startingIngredients[i].ingredient->color);
						else
							return;
							//MixLiquidColorWith(ingredientComp->startingIngredients[i].ingredient->color, 0.5f);
					}
				}
				else
					liquidMaterial->GetVectorParameterValue(TEXT("Liquid_Color"), currentLiquidColor);
			}
			if (currentFoamColor == FLinearColor(0, 0, 0, 0))//Making sure it's not already changed by the server for late joining players
				liquidMaterial->GetVectorParameterValue(TEXT("Foam_Color"), currentFoamColor);
		}
		if (!ingredientComp)
			InitMl(startingMl);
	}
	if (fillableInsideBody)
	{
		if (boxCollider)
		{
			if (boxCollider->GetCollisionResponseToChannel(ECollisionChannel::ECC_GameTraceChannel1))
			{
				boxCollider->OnComponentBeginOverlap.AddDynamic(this, &ULiquidContainerComp::OnFillBoxOverlap);
				boxCollider->OnComponentEndOverlap.AddDynamic(this, &ULiquidContainerComp::OnOverlapEnd);
			}
		}
	}
	if (capAttachPoint)
	{
		capAttachPoint->OnObjectAttached.AddDynamic(this, &ULiquidContainerComp::OnCapAttached);
		capAttachPoint->OnObjectAttached.AddDynamic(this, &ULiquidContainerComp::OnCapDetached);
	}
}

void ULiquidContainerComp::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!IsRunningDedicatedServer())
	{
		ShouldUseMeniscus();
		if (!genericHandOwner)
			return;

		if (genericHandOwner->isGrabbed)
		{
			if (drippable && boxCollider)
				CheckForDrip(DeltaTime);

			if (fillingFrom)
			{
				UIngredientsContainerComponent* otherIngredientComp = Utility::GetComponentUtility<UIngredientsContainerComponent>(fillingFrom);
				if (CanAcceptLiquidFrom(otherIngredientComp))
				{
					float fillAmount = fillingSpeed * DeltaTime;
					AddMl(fillAmount);
					if (ingredientComp)
					{
						if (otherIngredientComp)
							otherIngredientComp->TransferAllIngredientsTo(this, fillAmount);
					}
				}
			}
			float velocityMagnitude = CalculateVelocityMagnitude(lastFramePosition);
			float velocity = velocityMagnitude * DeltaTime;
			if (velocity > shakeTreshold && CanAddShakeAmount())
			{
				++currentShakeAmount;
				if (currentShakeAmount >= shakeAmountToComplete)
				{
					Shake();
					OnShaked();
				}
			}
			lastFrameVelocity = velocity;
		}
		else
		{
			ActivateDeactivateDrip(false);
		}
	}
}
#pragma endregion inherited
#pragma region dripping
void ULiquidContainerComp::CheckForDrip(float deltaTime)
{
	UpdateMeshBottom();
	if (meshBottom != FVector(0, 0, 0))
	{
		//DrawDebugLine(GetWorld(), meshBottom, meshCenter, FColor::Yellow, false, 0.01, 0); // <-- for debuging the raycast
		UpdateTopLiquidXLocation();
		FVector objectDirection = ((boxCollider->GetComponentLocation() - meshBottom) * 10) + GetOwner()->GetActorLocation();
		FHitResult hitResult;
		FVector traceEnd = FVector(objectDirection.X, objectDirection.Y, topLiquidXLocation.Z);
		FCollisionQueryParams params = FCollisionQueryParams(FName("LiquidPouring"));
		//DrawDebugLine(GetWorld(), meshBottom, boxCollider->GetComponentLocation(), FColor::Green, false, 0.01, 0); // <-- for debuging the raycast
		//DrawDebugLine(GetWorld(), traceStart, traceEnd, FColor::Green, false, 0.01, 0); // <-- for debuging the raycast
		float minDripSpeed = 1;
		float maxDripSpeed = precisionDripping ? maxMl * 0.5 : maxMl;
		if (GetWorld()->LineTraceSingleByChannel(hitResult, topLiquidXLocation, traceEnd, ECC_GameTraceChannel1, params))
		{
			if (hitResult.GetComponent()->ComponentHasTag("LiquidPour") && hitResult.GetActor() == GetOwner())
			{
				FVector hitCompLocation = hitResult.Component->GetComponentLocation();
				UBoxComponent* hitComponent = Cast<UBoxComponent>(hitResult.Component);
				float hitToCenterDistance = FVector::Dist(hitResult.Location, hitCompLocation);
				if (hitToCenterDistance > maxDripDistanceToCenter)
					maxDripDistanceToCenter = hitToCenterDistance;
				float desiredDripSpeed = 0;
				if (FMath::Abs(GetOwner()->GetActorRotation().Euler().Y) > 90 || FMath::Abs(GetOwner()->GetActorRotation().Euler().X) > 90)
					desiredDripSpeed = maxDripSpeed;
				else
				{
					float dripAlpha = 1 - (hitToCenterDistance * 1 / maxDripDistanceToCenter);
					desiredDripSpeed = FMath::Lerp(minDripSpeed, maxDripSpeed, dripAlpha);
				}
				SetNiagaraLocation(hitResult.Location);
				DripDetected(deltaTime, hitResult.Location, desiredDripSpeed);
			}
		}
		else if (FMath::Abs(GetOwner()->GetActorRotation().Euler().Y) > 90 || FMath::Abs(GetOwner()->GetActorRotation().Euler().X) > 90)
		{
			SetNiagaraLocation(boxCollider->GetComponentLocation());
			DripDetected(deltaTime, boxCollider->GetComponentLocation(), maxDripSpeed);
		}
		else
		{
			niagaraLocationSet = false;
			ActivateDeactivateDrip(false);
		}
	}
}

void ULiquidContainerComp::DripDetected(float deltaTime, FVector dripLocation, float dripSpeed)
{
	bool shouldDrip = true;
	bool shouldTransferLiquid = true;
	ULiquidContainerComp* otherContainer = GetContainerBelow(dripLocation);
	if (!otherContainer)
		shouldTransferLiquid = false;
	else if(!otherContainer->CanAcceptLiquidFrom(ingredientComp))
			shouldTransferLiquid = false;
	
	if (!shouldTransferLiquid && dripsOnlyIfContainer)
		shouldDrip = false;

	if (currentMl <= 0)
	{
		if (ingredientComp)
			ingredientComp->S_ClearAllIngredientsAdded();

		shouldDrip = false;
	}

	if (shouldDrip)
	{
		float dripAmount;
		float dynamicDrippingSpeed = precisionDrippingSpeed;
		if (precisionDripping)
		{
			if (otherContainer && otherContainer->precisionDripping)
				if (otherContainer->precisionDrippingSpeed < precisionDrippingSpeed)
					dynamicDrippingSpeed = otherContainer->precisionDrippingSpeed;

			dripAmount = deltaTime * dripSpeed * dynamicDrippingSpeed;
		}
		else
			dripAmount = deltaTime * dripSpeed;

		ChangeMl(currentMl - dripAmount);
		if (!otherContainer && ingredientComp)
			ingredientComp->RemoveIngredientAmountFromAll(dripAmount);
			
		if (shouldTransferLiquid)
		{
			interactedLiquidContainer = otherContainer;
			if (!otherContainer->GetOwner()->GetOwner())
			{
				AActor* owner = GetOwner()->GetOwner();
				APawnProteusBase::Delegate nullDelegate;
				Cast<APawnProteusBase>(owner)->ChangeActorOwner(otherContainer->GetOwner(), owner, nullDelegate);
			}
			if (otherContainer->GetCurrentMl() <= 0.1)
				otherContainer->ChangeLiquidColor(currentLiquidColor);
				
			float NewTemp = (otherContainer->CurrentTemperature * otherContainer->GetCurrentMl() + CurrentTemperature * dripAmount) / (otherContainer->GetCurrentMl() + dripAmount);
			otherContainer->S_SetCurrentTemp(NewTemp);

			otherContainer->ChangeMl(otherContainer->currentMl + dripAmount);
			if (ingredientComp)
			{
				UIngredientsContainerComponent* otherIngredientContainer = Utility::GetComponentUtility<UIngredientsContainerComponent>(otherContainer->GetOwner());
				if (otherIngredientContainer)
					ingredientComp->TransferAllIngredientsTo(otherContainer, dripAmount);
			}
		}
		ActivateDeactivateDrip(true, dripLocation);
		if(OnDrip.IsBound())
			OnDrip.Broadcast(otherContainer, dripAmount);
	}
	else
	{
		ActivateDeactivateDrip(false);
	}
}

void ULiquidContainerComp::ActivateDeactivateDrip(bool activate, FVector dripLocation)
{
	if (dripping == activate)
		return;

	dripping = activate;
	if (dripping)
	{
		//niagara->SetWorldLocation(dripLocation);
		drippingNiagara->Activate();
		if (drippingAudio)
			drippingAudio->Play();
	}
	else
	{
		drippingNiagara->Deactivate();
		if (drippingAudio)
			drippingAudio->Stop();
		if (interactedLiquidContainer)
		{
			AActor* owner = interactedLiquidContainer->GetOwner()->GetOwner();
			if (owner && interactedLiquidContainer->genericHandOwner && !interactedLiquidContainer->genericHandOwner->isGrabbed)
			{
				APawnProteusBase::Delegate nullDelegate;
				Cast<APawnProteusBase>(owner)->ChangeActorOwner(interactedLiquidContainer->GetOwner(), nullptr, nullDelegate);
			}
			interactedLiquidContainer = nullptr;
		}
	}
}

void ULiquidContainerComp::SetNiagaraLocation(FVector newLocation)
{
	if (niagaraLocationSet)
		return;

	niagaraLocationSet = true;
	drippingNiagara->SetWorldLocation(newLocation);
}

ULiquidContainerComp* ULiquidContainerComp::GetContainerBelow(FVector from)
{
	FHitResult hitResult;
	FVector traceStart = FVector(0, 0, 0);
	from == FVector(0, 0, 0) ? traceStart = Utility::GetComponentUtility<USceneComponent>(GetOwner())->GetComponentLocation() : traceStart = from;
	FVector traceEnd = FVector(traceStart.X, traceStart.Y, traceStart.Z - 50.0f);
	//DrawDebugLine(GetWorld(), traceStart, traceEnd, FColor::Red, false, 0.01, 0); // <-- for debuging the raycast
	FCollisionQueryParams params = FCollisionQueryParams("ContainerBelow", false, GetOwner());
	if (GetWorld()->LineTraceSingleByChannel(hitResult, traceStart, traceEnd, ECC_Visibility, params))
	{
		ULiquidContainerComp* otherLiquidContainer = Utility::GetComponentUtility<ULiquidContainerComp>(hitResult.GetActor());
		if (otherLiquidContainer)
			return otherLiquidContainer;
	}
	return nullptr;
}

void ULiquidContainerComp::InitPouring()
{
	if (IsRunningDedicatedServer())
		return;
	TArray<USceneComponent*> sceneComps;
	GetOwner()->GetComponents<USceneComponent>(sceneComps);
	for (int i = 0; i < sceneComps.Num(); ++i)
	{
		if (sceneComps[i]->ComponentHasTag("LowestPoint"))
		{
			liquidXLowestPoint = sceneComps[i];
			break;
		}
	}
	TArray<UNiagaraComponent*> niagaraComps;
	GetOwner()->GetComponents<UNiagaraComponent>(niagaraComps);
	for (int i = 0; i < niagaraComps.Num(); ++i)
	{
		if (niagaraComps[i]->GetAsset()->GetFName().ToString() == "BasicSystem")
		{
			drippingNiagara = niagaraComps[i];
			drippingNiagara->Deactivate();
			if (liquidMaterial)
			{
				FLinearColor liquidColor;
				liquidMaterial->GetVectorParameterValue(TEXT("Foam_Color"), liquidColor);
				drippingNiagara->SetColorParameter("User.Color", liquidColor);
			}
		}
		else
		{
			bubblesNiagara = niagaraComps[i];
			EnableBubbles(false);
		}
	}
}
#pragma endregion dripping
#pragma region shake
float ULiquidContainerComp::CalculateVelocityMagnitude(FVector lastPosition)
{
	lastFramePosition = GetOwner()->GetActorLocation();
	if (lastPosition.IsZero())
		return 0;

	FVector currentPosition = GetOwner()->GetActorLocation();
	FVector currentDirection = lastPosition - currentPosition;
	return currentDirection.Size();
}

bool ULiquidContainerComp::CanAddShakeAmount()
{
	return lastFrameVelocity < shakeTreshold;
}

void ULiquidContainerComp::Shake()
{
	currentShakeAmount = 0;
	if (shaken)
		return;
	if (!ingredientComp)
		return;

	if (ingredientComp->IsAnyRecipeMade())
	{
		shaken = true;
		FLinearColor currentColor = FLinearColor::White;
		liquidMaterial->GetVectorParameterValue(TEXT("Foam_Color"), currentColor);
		float colorFactor = 10 / GetCurrentMl();
		FLinearColor colorToApply = FColor::White;
		bool hasPhenol = ingredientComp->HasIngredient("Red Phenol");
		bool hasBromocresol = ingredientComp->HasIngredient("Bromocresol blue");
		TArray<ULiquidIngredientData*> addedPhIngredients;
		for (int i = 0; i < ingredientComp->ingredientsAdded.Num(); ++i)
		{
			ULiquidIngredientData* liquidIngredientTest = Cast<ULiquidIngredientData>(ingredientComp->ingredientsAdded[i].ingredient);
			if (liquidIngredientTest)
			{
				if (liquidIngredientTest->uniqueName == "Red Phenol" || liquidIngredientTest->uniqueName == "Bromocresol blue")
					continue;

				addedPhIngredients.Add(liquidIngredientTest);
				if (hasBromocresol && hasPhenol)
				{
					colorToApply = FColor::Black;
					break;
				}
				if (addedPhIngredients.Num() == 1)
				{
					if (hasBromocresol)
						colorToApply = liquidIngredientTest->phData->bromocresolColor;
					else if (hasPhenol)
						colorToApply = liquidIngredientTest->phData->redPhenolColor;
				}
				else if (addedPhIngredients.Num() > 1)
				{
					FLinearColor mixingColor;
					if (hasBromocresol)
						mixingColor = liquidIngredientTest->phData->bromocresolColor;
					else if (hasPhenol)
						mixingColor = liquidIngredientTest->phData->redPhenolColor;

					colorToApply = colorToApply + 0.5f * (mixingColor - colorToApply);
				}
			}
		}
		FLinearColor linearPhColor = colorToApply;
		FLinearColor lerpedColour = currentColor + colorFactor * (linearPhColor - currentColor);
		ChangeLiquidColor(lerpedColour);
		ShakenUpdateJournal(addedPhIngredients, ingredientComp->containerName);
	}
}
#pragma endregion shake
#pragma region mlChanges
void ULiquidContainerComp::ChangeMl(float ml, bool updateMaterial)
{
	if (ml > maxMl)
		ml = maxMl;
	else if (ml <= 0)
	{
		ml = 0;
		if (ingredientComp)
			ingredientComp->ClearIngredients();
	}
	currentMl = ml;
	UpdateBubblesNiagaraKillZone();
	if (GetOwner()->HasNetOwner())
		S_SetCurrentMl(currentMl);
	if (updateMaterial)
		UpdateLiquidMaterial(ml);
	if (OnChangedMl.IsBound())
		OnChangedMl.Broadcast();
}

void ULiquidContainerComp::AddMl(float ml, bool updateMaterial)
{
	ChangeMl(currentMl + ml, updateMaterial);
}

void ULiquidContainerComp::OnFillBoxOverlap(class UPrimitiveComponent* OverlappedComp, class AActor* OtherActor, class UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (IsRunningDedicatedServer())
		return;

	AGenericHandObjectBase* owner = Cast<AGenericHandObjectBase>(GetOwner());
	if (!owner || !owner->isGrabbed)
		return;

	if (OtherComp->ComponentHasTag("Fill"))
	{
		UIngredientsContainerComponent* otherIngredientContainer = Utility::GetComponentUtility<UIngredientsContainerComponent>(OtherActor);
		if (CanAcceptLiquidFrom(otherIngredientContainer))
		{
			fillingFrom = OtherActor;
			OnFill();
		}
	}
}

void ULiquidContainerComp::OnOverlapEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (OtherComp->ComponentHasTag("Fill"))
	{
		fillingFrom = nullptr;
		OnEndFill();
	}
}

void ULiquidContainerComp::InitMl(float desiredMl)
{ 
	if (!liquidMaterial)
		InitLiquidMaterial();
	if (liquidMaterial)
		//Checking if it's == -1 to make sure it's not already changed by the server for late joining players
		currentMl == -1 ? ChangeMl(desiredMl) : UpdateLiquidMaterial(currentMl);
}

bool ULiquidContainerComp::CanAcceptLiquidFrom(UIngredientsContainerComponent* fromContainer)
{
	if (!CanAcceptLiquid())
		return false;

	if (fromContainer && ingredientComp)
		for (int i = 0; i < fromContainer->ingredientsAdded.Num(); ++i)
			if (!ingredientComp->CanAddIngredient(fromContainer->ingredientsAdded[i].ingredient))
				return false;

	return true;
}

bool ULiquidContainerComp::CanAcceptLiquid()
{
	bool acceptLiquid = true;
	if (GetCurrentMl() >= maxMl)
		acceptLiquid = false;
	else if (hasCap)
		acceptLiquid = false;
	
	return acceptLiquid;
}

float ULiquidContainerComp::GetCurrentMl()
{
	return currentMl;
}
#pragma endregion mlChanges
#pragma region liquidX
void ULiquidContainerComp::ChangeLiquidColor(FLinearColor liquidColor, bool shouldReplicate)
{
	currentFoamColor = liquidColor;
	currentLiquidColor = liquidColor;
	if (!liquidMaterial)
		return;
	liquidMaterial->SetVectorParameterValue(TEXT("Foam_Color"), liquidColor);
	liquidMaterial->SetVectorParameterValue(TEXT("Liquid_Color"), liquidColor);
	if (shouldReplicate)
	{
		if (GetOwner()->HasNetOwner() || GetOwner()->HasAuthority())
			S_SetCurrenColor(liquidColor);
	}
	if(drippingNiagara && !IsRunningDedicatedServer())
		drippingNiagara->SetColorParameter("User.Color", liquidColor);
}

void ULiquidContainerComp::ChangeFoamColor(FLinearColor foamColor)
{
	currentFoamColor = foamColor;
	liquidMaterial->SetVectorParameterValue(TEXT("Foam_Color"), foamColor);
	Cast<AGenericHandObjectBase>(GetOwner())->ChangeFoamColor(foamColor);
}

/*void ULiquidContainerComp::MixLiquidColorWith(FLinearColor secondaryColor, float factor)
{
	if (!liquidMaterial)
		return;

	FLinearColor currentColor;
	liquidMaterial->GetVectorParameterValue(TEXT("Liquid_Color"), currentColor);
	FLinearColor lerpedColor = currentColor + factor * (secondaryColor - currentColor);
	ChangeLiquidColor(lerpedColor);
}*/

void ULiquidContainerComp::InitLiquidMaterial()
{
	if (liquidMaterial)
		return;

	TArray<UStaticMeshComponent*> meshs;
	GetOwner()->GetComponents<UStaticMeshComponent>(meshs);
	UStaticMeshComponent* goodMesh = nullptr;
	for (int i = 0; i < meshs.Num(); ++i)
	{
		if (meshs[i] && meshs[i] != nullptr && meshs[i]->GetMaterials().Num() > 0 && meshs[i]->GetMaterials()[0]->GetBaseMaterial() != nullptr)
		{
			if (meshs[i]->GetMaterials()[0]->GetBaseMaterial()->GetName() == "DynamicLiquidFX1")
			{
				goodMesh = meshs[i];
				break;
			}
		}
	}
	if (!goodMesh)
		return;

	UMaterialInstanceDynamic* dynMat = UMaterialInstanceDynamic::Create(goodMesh->GetMaterial(0), this);
	goodMesh->SetMaterial(0, dynMat);
	liquidMaterial = dynMat;
}

void ULiquidContainerComp::UpdateLiquidMaterial(float ml)
{
	if (liquidMaterial)
		liquidMaterial->SetScalarParameterValue(TEXT("Liquid_FillHeight"), GetLiquidXHeight(ml));
}

float ULiquidContainerComp::GetLiquidXHeight(float ml)
{
	AdjustLiquidXMax();
	float minMatLiquid = -0.5f;
	float maxMatLiquid = liquidXMax;
	return FMath::GetMappedRangeValueClamped(FVector2D(0, maxMl), FVector2D(minMatLiquid, maxMatLiquid), ml);
}

void ULiquidContainerComp::OnOffLiquidX(bool on)
{
	if (liquidMaterial)
		liquidMaterial->SetScalarParameterValue(TEXT("Liquid_FillHeight"), on ? GetLiquidXHeight(currentMl) : -0.5);
}

FVector ULiquidContainerComp::UpdateMeshBottom()
{
	FBox boundingBox = GetOwner()->GetComponentsBoundingBox();
	FVector meshCenter = boundingBox.GetCenter();
	FVector traceEnd = FVector(meshCenter.X, meshCenter.Y, meshCenter.Z - 30);
	//2 different ways to find the lowestPoint on the mesh, figured that with how liquidX is made one was better when close to empty
	//while the other was better when close to full, so mixing both depending on the currentMl
	if (currentMl < maxMl / 2)
	{
		//take the lowest point depending on orientation
		TArray<FHitResult> lowestPointHit;
		//DrawDebugLine(GetWorld(), traceEnd, meshCenter, FColor::Yellow, false, 0.01, 0); // <-- for debuging the raycast
		if (GetWorld()->LineTraceMultiByObjectType(lowestPointHit, traceEnd, meshCenter, ECC_PhysicsBody))
		{
			for (int i = 0; i < lowestPointHit.Num(); ++i)
			{
				if (lowestPointHit[i].GetActor() == GetOwner())
				{
					meshBottom = lowestPointHit[i].ImpactPoint;
					break;
				}
			}
		}
	}
	else if (liquidXLowestPoint)
	{
		//take the lowest point without looking at the orientation
		meshBottom = liquidXLowestPoint->GetComponentLocation();
	}
	return meshBottom;
}

void ULiquidContainerComp::UpdateTopLiquidXLocation()
{
	if (!boxCollider)
		return;

	float liquidXHeight = GetLiquidXHeight(currentMl);
	liquidActualHeight = FMath::GetMappedRangeValueClamped(FVector2D(-0.5, liquidXMax), FVector2D(meshBottom.Z, boxCollider->GetComponentLocation().Z), liquidXHeight); //liquidHeight in world coordinates
	topLiquidXLocation = FVector(meshBottom.X, meshBottom.Y, liquidActualHeight);
}
#pragma endregion liquidX
#pragma region misc
void ULiquidContainerComp::UpdateBubblesNiagaraKillZone()
{
	if (bubblesNiagara)
	{
		UpdateMeshBottom();
		if (meshBottom != FVector(0, 0, 0))
		{
			UStaticMeshComponent* mesh = Cast<UStaticMeshComponent>(GetOwner()->GetRootComponent());
			if (mesh)
			{
				UpdateTopLiquidXLocation();
				FVector boundingBox = mesh->GetStaticMesh()->GetBoundingBox().Min;
				float topOfLiquidHeight = topLiquidXLocation.Z - meshBottom.Z;
				if (topOfLiquidHeight < 0)
					topOfLiquidHeight = 0;
				FVector newKillBoxSize = FVector(boundingBox.X, boundingBox.Y, topOfLiquidHeight);
				bubblesNiagara->SetNiagaraVariableVec3("KillBoxSize", newKillBoxSize);
				//if(!IsRunningDedicatedServer())
					//DrawDebugBox(GetWorld(), bubblesNiagara->GetComponentLocation(), newKillBoxSize, FColor::Red, false, 10); //To debug the killBoxSize
			}
		}
	}
}

void ULiquidContainerComp::EnableBubbles(bool enable)
{
	if (!bubblesNiagara)
		return;
	if (currentMl <= 0)
		enable = false;

	enable ? bubblesNiagara->Activate() : bubblesNiagara->Deactivate();
}

void ULiquidContainerComp::OnCapAttached(AGenericHandObjectBase* objectRef)
{
	hasCap = true;
}

void ULiquidContainerComp::OnCapDetached(AGenericHandObjectBase* objectRef)
{
	hasCap = false;
}
#if WITH_EDITOR
bool ULiquidContainerComp::CanEditChange(const FProperty* InProperty) const
{
	const bool parentValue = Super::CanEditChange(InProperty);
	if (InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(ULiquidContainerComp, startingMl))
		if(GetOwner())
			return parentValue && !Utility::GetComponentUtility<UIngredientsContainerComponent>(GetOwner());

	return parentValue;
}
#endif
#pragma endregion misc
#pragma region Online
void ULiquidContainerComp::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ULiquidContainerComp, currentMl);
	DOREPLIFETIME(ULiquidContainerComp, currentLiquidColor);
	DOREPLIFETIME(ULiquidContainerComp, currentFoamColor);
	DOREPLIFETIME(ULiquidContainerComp, CurrentTemperature);
	DOREPLIFETIME(ULiquidContainerComp, CurrentConcentration);
}

void ULiquidContainerComp::OnRepCurrentMl()
{
	ChangeMl(currentMl);
}

void ULiquidContainerComp::S_SetCurrentMl_Implementation(float ml)
{
	currentMl = ml;
}

void ULiquidContainerComp::S_SetCurrentTemp_Implementation(float NewTemp)
{
	CurrentTemperature = NewTemp;
}

void ULiquidContainerComp::OnRepCurrentColor()
{
	if (IsRunningDedicatedServer())
		return;

	if (!liquidMaterial) //some late joining clients could run this before the beginPlay()
	{
		FTimerHandle timerHandle;
		FTimerDelegate timerDelegate;
		timerDelegate.BindUObject(this, &ULiquidContainerComp::OnRepCurrentColor);
		GetWorld()->GetTimerManager().SetTimer(timerHandle, timerDelegate, 0.1, false);
		return;
	}
	liquidMaterial->SetVectorParameterValue(TEXT("Foam_Color"), currentLiquidColor);
	liquidMaterial->SetVectorParameterValue(TEXT("Liquid_Color"), currentLiquidColor);
	if (drippingNiagara)
		drippingNiagara->SetColorParameter("User.Color", currentLiquidColor);
}

void ULiquidContainerComp::OnRepFoamColor()
{
	ChangeFoamColor(currentFoamColor);
}

void ULiquidContainerComp::S_SetCurrenColor_Implementation(FLinearColor newColor)
{
	currentLiquidColor = newColor;
}
#pragma endregion Online