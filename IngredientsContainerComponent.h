#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Proteus/GeneralProject/Pawn/PawnProteusBase.h"
#include "Proteus/Objects/Data/Recipe/RecipeData.h"
#include "Proteus/Objects/Data/Ingredient/IngredientData.h"
#include "Proteus/Objects/MixCalculator/MixCalculator.h"
#include "IngredientsContainerComponent.generated.h"

class ULiquidContainerComp;

UENUM(BlueprintType)
enum class EIngredientType : uint8
{
	ANY,
	LIQUID,
	SOLID
};

USTRUCT(BlueprintType)
struct FRuntimeIngredientData
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
		UIngredientData* ingredient = nullptr;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
		float quantity;
};

UCLASS( ClassGroup=(Custom), EditInlineNew, Blueprintable, meta=(BlueprintSpawnableComponent) )
class PROTEUS_API UIngredientsContainerComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UIngredientsContainerComponent();
	static TArray<URecipeData*> allRecipes;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayThumbnail = "false", ToolTip = "Add all recipes that could be made in this container"))
		TArray<URecipeData*> acceptableRecipes;
	UPROPERTY(Replicated, BlueprintReadWrite)
		URecipeData* completedRecipes;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ToolTip = "Add all ingredients that this container should contain on begin play"))
		TArray<FRuntimeIngredientData> startingIngredients;
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
		FString containerName;
	UPROPERTY(Replicated, BlueprintReadWrite)
		TArray<FRuntimeIngredientData> ingredientsAdded;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ToolTip = "-1 = infinite"))
		int maxSolids = 1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ToolTip = "-1 = infinite"))
		int maxLiquids = 1;

	UFUNCTION(BlueprintCallable, meta = (ToolTip = "Used to automatically compute the whole simulation at once, look into MHA.cpp for proper documentation"))
		void RunMixSimulation(float temp, float pressure, float timeStep);
	UFUNCTION(BlueprintCallable)
		virtual void AddIngredient(FRuntimeIngredientData ingredient);
	UFUNCTION(BlueprintCallable, BlueprintPure)
		bool IsAnyRecipeMade();
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "IngredientsContainerComponent", meta = (ToolTip = "Click to add all recipes in the project to the acceptableRecipes array"))
		void AcceptAllRecipes();
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "IngredientsContainerComponent", meta = (ToolTip = "Click to copy the acceptable recipes array"))
		void CopyRecipes();
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "IngredientsContainerComponent", meta = (ToolTip = "Click to paste the copied acceptable recipes array"))
		void PasteRecipes();
	UFUNCTION(BlueprintCallable)
		void TransferAndClearAllIngredientsTo(UIngredientsContainerComponent* receiver);
	UFUNCTION(BlueprintCallable)
		void TransferAllIngredientsToByAmount(UIngredientsContainerComponent* receiver, float totalDripAmount);
	UFUNCTION(BlueprintCallable)
		bool HasIngredient(FString ingredientToFind);
	UFUNCTION(Server, Reliable)
		void S_SetIngredientAdded(FRuntimeIngredientData newIngredient);
	UFUNCTION(Server, Reliable)
		void S_ClearAllIngredientsAdded();
	UFUNCTION(BlueprintCallable)
		void ClearIngredients();
	UFUNCTION(BlueprintCallable)
		void ClearIngredient(UIngredientData* ingredient);
	UFUNCTION(BlueprintCallable)
		void RemoveIngredientAmount(UIngredientData* ingredient, float quantity);
	UFUNCTION(BlueprintCallable)
		void RemoveIngredientAmountFromAll(float totalQuantityRemoved);
	UFUNCTION(BlueprintCallable)
		void AddRemoveIngredientAmount(FRuntimeIngredientData ingredient);
	UFUNCTION(BlueprintCallable, BlueprintPure)
		float GetTotalQuantity(EIngredientType ingredientFilter);
	UFUNCTION(BlueprintCallable, BlueprintPure)
		int GetNumberOfIngredients(EIngredientType ingredientFilter);
	UFUNCTION(BlueprintCallable, BlueprintPure)
		bool CanAddIngredient(UIngredientData* ingredient);
	UFUNCTION(Server, Reliable)
		void S_RemoveIngredient(UIngredientData* ingredient, float quantity);
	
	void TransferAllIngredientsTo(ULiquidContainerComp* receiver, float totalDripAmount);

protected:
	virtual void BeginPlay() override;
	void LookIfRecipeWasMade(UIngredientData* lastAddedIngredient);
	bool GetRuntimeIngredient(UIngredientData* ingredient, FRuntimeIngredientData &outIngredient);
	int GetRuntimeIngredientIndex(UIngredientData* ingredient);
	void AddIngredient(UIngredientData* ingredient, float amount);
	UFUNCTION(BlueprintCallable, BlueprintPure)
		URecipeData* GetRecipeByName(FString name);
	UFUNCTION(BlueprintCallable, BlueprintPure)
		bool IsRecipeMade(URecipeData* recipe);

private:
	void FetchAllPossibleRecipes();
	static TArray<URecipeData*> copiedRecipeArray;
};