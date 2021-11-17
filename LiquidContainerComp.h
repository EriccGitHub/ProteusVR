#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Components/BoxComponent.h"
#include "Components/AudioComponent.h"
#include "TimerManager.h"
#include "Niagara/Public/NiagaraComponent.h"
#include "Proteus/GeneralProject/Utility.h"
#include "Proteus/Actors/GenericObjects/GenericHandObjectBase.h"
#include "Proteus/Actors/GenericObjects/GenericAttachPointBase.h"
#include "Proteus/Components/RecipesAndIngredients/IngredientsContainerComponent.h"
#include "LiquidContainerComp.generated.h"

UCLASS( ClassGroup=(Custom), Blueprintable, meta = (BlueprintSpawnableComponent))
class PROTEUS_API ULiquidContainerComp : public UActorComponent
{
	GENERATED_BODY()

#if WITH_EDITOR
		virtual bool CanEditChange(const FProperty* InProperty) const override;
#endif
public:
	ULiquidContainerComp();
	UFUNCTION(BlueprintCallable)
		virtual void BeginPlay() override;

private:
	float CalculateVelocityMagnitude(FVector lastPosition);
	FVector lastFramePosition;
	float shakeTreshold = 0.04f;
	int shakeAmountToComplete = 3;
	int currentShakeAmount = 0;
	float lastFrameVelocity = 0;
	bool CanAddShakeAmount();
	float maxDripDistanceToCenter = 0;
	USceneComponent* liquidXLowestPoint;

protected:
	UPROPERTY(BlueprintReadOnly, Replicated, ReplicatedUsing = OnRepCurrentMl)
		float currentMl = -1;
	AActor* fillingFrom;
	bool dripping = false;
	bool niagaraLocationSet = false;
	bool shaken = false;
	UBoxComponent* boxCollider;
	UNiagaraComponent* drippingNiagara = nullptr;
	UNiagaraComponent* bubblesNiagara = nullptr;
	UMaterialInstanceDynamic* niagaraMaterial = nullptr;
	AGenericHandObjectBase* genericHandOwner;
	UAudioComponent* drippingAudio;
	virtual void InitLiquidMaterial();
	void SetNiagaraLocation(FVector newLocation);
	void InitPouring();
	void UpdateLiquidMaterial(float ml);
	void CheckForDrip(float deltaTime);
	void DripDetected(float deltaTime, FVector dripLocation, float dripSpeed);
	void ActivateDeactivateDrip(bool activate, FVector dripLocation = FVector(0, 0, 0));
	FVector UpdateMeshBottom();
	FVector meshBottom = FVector(0, 0, 0);
	void UpdateTopLiquidXLocation();
	void UpdateBubblesNiagaraKillZone();
	float GetLiquidXHeight(float ml);
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	ULiquidContainerComp* GetContainerBelow(FVector from = FVector(0, 0, 0));
	virtual void Shake();
	UFUNCTION()
		void OnRepCurrentMl();
	UFUNCTION()
		void OnRepCurrentColor();
	UFUNCTION()
		void OnRepFoamColor();
	UFUNCTION(Server, Reliable)
		void S_SetCurrentMl(float ml);
	UFUNCTION(Server, Reliable)
		void S_SetCurrentTemp(float NewTemp);

public:
	UPROPERTY(BlueprintReadOnly, Replicated, ReplicatedUsing = OnRepCurrentColor)
		FLinearColor currentLiquidColor = FLinearColor(0, 0, 0, 0);
	UPROPERTY(Replicated, ReplicatedUsing = OnRepFoamColor)
		FLinearColor currentFoamColor = FLinearColor(0, 0, 0, 0);
	UPROPERTY(BlueprintReadWrite, Replicated)
		float CurrentTemperature = 0;
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Replicated)
		float CurrentConcentration = 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
		float maxMl = 250;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
		float startingMl = 0;
	UPROPERTY(EditAnywhere, meta = (ToolTip = "If enabled, the container can be filled when the top enters in contact with anything that has the tag 'Fill'"))
		bool fillableInsideBody = false;
	UPROPERTY(EditAnywhere, meta = (EditCondition = "fillableInsideBody", EditConditionHides))
		float fillingSpeed = 300;
	UPROPERTY(EditAnywhere, meta = (ToolTip = "If enabled, the container can drip if angled enough"))
		bool drippable = true;
	UPROPERTY(EditAnywhere, meta = (EditCondition = "drippable", EditConditionHides, ToolTip = "If enabled, the container can drip only if there's an other liquid container below it to receive the liquid"))
		bool dripsOnlyIfContainer = true;
	UPROPERTY(EditAnywhere, meta = (EditCondition = "drippable", EditConditionHides, ToolTip = "If enabled, the dripping speed will be multiplied by precision dripping speed or by the one from the container dripping into, whichever is smaller"))
		bool precisionDripping = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditCondition = "precisionDripping && drippable", EditConditionHides, ToolTip = "Dripping speed from 0 to 1"))
		float precisionDrippingSpeed = 1;
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (ToolTip = "Where the liquid should stop acording to liquidX (0.5 = filling the mesh and -0.5 = always empty)"))
		float liquidXMax = 0.33;
	UPROPERTY(EditAnywhere, meta = (ToolTip = "The attach point that the cap of this container uses. Liquids won't be transfered to this container if the cap is attach to it."))
		AGenericAttachPointBase* capAttachPoint;
	UPROPERTY(BlueprintReadWrite)
		float liquidPoured;
	UPROPERTY(BlueprintReadWrite)
		UMaterialInstanceDynamic* liquidMaterial;
	UPROPERTY(BlueprintReadOnly)
		FVector topLiquidXLocation;
	UPROPERTY(BlueprintReadOnly)
		float liquidActualHeight = 0;
	UIngredientsContainerComponent* ingredientComp;
	float GetCurrentMl();
	//void MixLiquidColorWith(FLinearColor secondaryColor, float factor);
	void InitMl(float desiredMl);
	bool CanAcceptLiquidFrom(UIngredientsContainerComponent* fromContainer);
	bool CanAcceptLiquid();

	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FSimpleDelegate);
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FDrippingDelegate, ULiquidContainerComp*, interactedLiquidContainer, float, drippingAmount);

	UPROPERTY(BlueprintAssignable, BlueprintCallable)
		FSimpleDelegate OnChangedMl;
	UPROPERTY(BlueprintAssignable, BlueprintCallable)
		FDrippingDelegate OnDrip;
	UPROPERTY(BlueprintReadOnly)
		bool hasCap;
	UPROPERTY(BlueprintReadOnly)
		ULiquidContainerComp* interactedLiquidContainer;
	UFUNCTION(BlueprintCallable)
		void ChangeLiquidColor(FLinearColor liquidColor, bool shouldReplicate = true);
	UFUNCTION(BlueprintCallable)
		void ChangeFoamColor(FLinearColor foamColor);
	UFUNCTION(BlueprintCallable)
		void ChangeMl(float ml, bool updateMaterial = true);
	UFUNCTION(BlueprintCallable)
		void AddMl(float ml, bool updateMaterial = true);
	UFUNCTION(BlueprintCallable)
		void OnOffLiquidX(bool on);
	UFUNCTION(BlueprintCallable)
		void EnableBubbles(bool enable);
	UFUNCTION(BlueprintImplementableEvent)
		void OnFill();
	UFUNCTION(BlueprintImplementableEvent)
		void OnEndFill();
	UFUNCTION(BlueprintImplementableEvent)
		void ShouldUseMeniscus();
	UFUNCTION(BlueprintImplementableEvent)
		void AdjustLiquidXMax();
	UFUNCTION(Server, Reliable, BlueprintCallable)
		void S_SetCurrenColor(FLinearColor newColor);
	UFUNCTION()
		virtual void OnFillBoxOverlap(class UPrimitiveComponent* OverlappedComp, class AActor* OtherActor, class UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
	UFUNCTION()
		void OnOverlapEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);
	UFUNCTION()
		void OnCapAttached(AGenericHandObjectBase* objectRef);
	UFUNCTION()
		void OnCapDetached(AGenericHandObjectBase* objectRef);
	UFUNCTION(BlueprintImplementableEvent)
		void ShakenUpdateJournal(const TArray<ULiquidIngredientData*>& allPhIngredients, const FString& fromContainer);
	UFUNCTION(BlueprintImplementableEvent)
		void OnShaked();
};