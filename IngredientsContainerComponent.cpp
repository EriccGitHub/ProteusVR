#include "IngredientsContainerComponent.h"
#include <Runtime\Engine\Public\Net\UnrealNetwork.h>
#include "Proteus/Actors/GenericObjects/GenericHandObjectBase.h"
#include "AssetRegistryModule.h"
#include "Proteus/Objects/Data/Ingredient/LiquidIngredientData.h"
#include "Proteus/Objects/Data/Ingredient/SolidIngredientData.h"
#include "Proteus/Components/Liquid/LiquidContainerComp.h"
#include "Proteus/Components/SolidExchangeComponentBase.h"
#include "Proteus/Objects/MixCalculator/MixCalculator.h"
#include "Proteus/GeneralProject/Utility.h"

TArray<URecipeData*> UIngredientsContainerComponent::allRecipes;
TArray<URecipeData*> UIngredientsContainerComponent::copiedRecipeArray;

UIngredientsContainerComponent::UIngredientsContainerComponent()
{
	this->SetIsReplicatedByDefault(true);
}

void UIngredientsContainerComponent::BeginPlay()
{
	Super::BeginPlay();	
	if (allRecipes.Num() == 0)
		FetchAllPossibleRecipes();
	if (ingredientsAdded.Num() == 0) //if it's not a late joining client
		for (int i = 0; i < startingIngredients.Num(); ++i)
			ingredientsAdded.Add(startingIngredients[i]);

	ULiquidContainerComp* liquidComp = Utility::GetComponentUtility<ULiquidContainerComp>(GetOwner());
	if (liquidComp)
	{
		float totalLiquidQuanitity = GetTotalQuantity(EIngredientType::LIQUID);
		if(liquidComp->maxMl >= totalLiquidQuanitity)
			liquidComp->InitMl(totalLiquidQuanitity);
		else
			UE_LOG(LogTemp, Error, TEXT("The ingredientsContainerComponent has to much liquids quantity compared to it's LiquidContainerComp's maxMl on %s"), *GetOwner()->GetName());
	}
}

void UIngredientsContainerComponent::FetchAllPossibleRecipes()
{
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	TArray<FAssetData> foundRecipes;
	AssetRegistryModule.Get().GetAssetsByPath("/Game/ChemData/Ingredients/Recipes", foundRecipes, true);
	for (int i = 0; i < foundRecipes.Num(); ++i)
	{
		URecipeData* recipeAsset = Cast<URecipeData>(foundRecipes[i].GetAsset());
		if (recipeAsset)
			allRecipes.Add(recipeAsset);
	}
}

void UIngredientsContainerComponent::AddIngredient(FRuntimeIngredientData ingredient)
{
	S_SetIngredientAdded(ingredient);
}

void UIngredientsContainerComponent::AddIngredient(UIngredientData* ingredient, float amount)
{
	FRuntimeIngredientData newIngredient;
	newIngredient.ingredient = ingredient;
	newIngredient.quantity = amount;
	AddIngredient(newIngredient);
}

//Should be called on the server since completedRecipes is changed in here
void UIngredientsContainerComponent::LookIfRecipeWasMade(UIngredientData* lastAddedIngredient)
{
	for (int i = 0; i < acceptableRecipes.Num(); ++i)
	{
		if (acceptableRecipes[i]->ingredients.Contains(lastAddedIngredient))
		{
			bool recipeMade = true;
			for (int j = 0; j < acceptableRecipes[i]->ingredients.Num(); ++j)
			{
				if (!HasIngredient(acceptableRecipes[i]->ingredients[j]->uniqueName))
				{
					recipeMade = false;
					break;
				}
			}
			if (recipeMade)
			{
				completedRecipes = acceptableRecipes[i];
				acceptableRecipes[i]->recipeMade.Broadcast();
			}
		}
	}
}

void UIngredientsContainerComponent::ClearIngredient(UIngredientData* ingredient)
{
	FRuntimeIngredientData runtimeIngredient;
	if(GetRuntimeIngredient(ingredient, runtimeIngredient))
		RemoveIngredientAmount(ingredient, runtimeIngredient.quantity);
}

void UIngredientsContainerComponent::RemoveIngredientAmount(UIngredientData* ingredient, float quantity)
{
	S_RemoveIngredient(ingredient, quantity);
}

void UIngredientsContainerComponent::RemoveIngredientAmountFromAll(float totalQuantityRemoved)
{
	float totalQuantity = GetTotalQuantity(EIngredientType::ANY);
	for (int i = 0; i < ingredientsAdded.Num(); ++i)
	{
		float removedAmount = ingredientsAdded[i].quantity * totalQuantityRemoved / totalQuantity;
		RemoveIngredientAmount(ingredientsAdded[i].ingredient, removedAmount);
	}
}

void UIngredientsContainerComponent::AddRemoveIngredientAmount(FRuntimeIngredientData ingredient)
{
	if (ingredient.quantity > 0)
		AddIngredient(ingredient);
	else
		RemoveIngredientAmount(ingredient.ingredient, ingredient.quantity);
}

bool UIngredientsContainerComponent::GetRuntimeIngredient(UIngredientData* ingredient, FRuntimeIngredientData &outIngredient)
{
	int index = GetRuntimeIngredientIndex(ingredient);
	if (index == -1)
		return false;
	else
	{
		outIngredient = ingredientsAdded[index];
		return true;
	}
}

int UIngredientsContainerComponent::GetRuntimeIngredientIndex(UIngredientData* ingredient)
{
	for (int i = 0; i < ingredientsAdded.Num(); ++i)
		if (ingredientsAdded[i].ingredient->uniqueName == ingredient->uniqueName)
			return i;
	
	return -1;
}

bool UIngredientsContainerComponent::HasIngredient(FString ingredientToFind)
{
	for (int i = 0; i < ingredientsAdded.Num(); ++i)
		if (ingredientsAdded[i].ingredient->uniqueName == ingredientToFind)
			return true;

	return false;
}

bool UIngredientsContainerComponent::CanAddIngredient(UIngredientData* ingredient)
{
	bool canAddNewIngredient = true;
	ULiquidIngredientData* liquidIngredient = Cast<ULiquidIngredientData>(ingredient);
	if (liquidIngredient)
	{
		if(maxLiquids != -1)
			if (GetNumberOfIngredients(EIngredientType::LIQUID) >= maxLiquids && !liquidIngredient->IsIndicator)
				canAddNewIngredient = false;
	}
	else
	{
		if (maxSolids != -1)
			if (GetNumberOfIngredients(EIngredientType::SOLID) >= maxSolids)
				canAddNewIngredient = false;
	}
	if (canAddNewIngredient || HasIngredient(ingredient->uniqueName))
		return true;

	return false;
}

bool UIngredientsContainerComponent::IsRecipeMade(URecipeData* recipe)
{
	if (!recipe)
		return false;

	return completedRecipes == recipe;
}

bool UIngredientsContainerComponent::IsAnyRecipeMade()
{
	return completedRecipes != nullptr;
}

URecipeData* UIngredientsContainerComponent::GetRecipeByName(FString name)
{
	for (int i = 0; i < acceptableRecipes.Num(); ++i)
		if (acceptableRecipes[i]->uniqueName == name)
			return acceptableRecipes[i];

	return nullptr;
}

void UIngredientsContainerComponent::TransferAndClearAllIngredientsTo(UIngredientsContainerComponent* receiver)
{
	if (!receiver)
		return;
	for (int i = 0; i < ingredientsAdded.Num(); ++i)
		receiver->AddIngredient(ingredientsAdded[i]);

	ClearIngredients();
}

void UIngredientsContainerComponent::TransferAllIngredientsTo(ULiquidContainerComp* receiver, float totalDripAmount)
{
	if (!receiver)
		return;
	UIngredientsContainerComponent* ingredientReceiver = Utility::GetComponentUtility<UIngredientsContainerComponent>(receiver->GetOwner());
	if (!ingredientReceiver)
		return;

	TransferAllIngredientsToByAmount(ingredientReceiver, totalDripAmount);
}

void UIngredientsContainerComponent::TransferAllIngredientsToByAmount(UIngredientsContainerComponent* receiver, float totalDripAmount)
{
	float totalQuantity = GetTotalQuantity(EIngredientType::ANY);
	for (int i = 0; i < ingredientsAdded.Num(); ++i)
	{
		float transferedAmount = ingredientsAdded[i].quantity * totalDripAmount / totalQuantity;
		if(receiver)
			receiver->AddIngredient(ingredientsAdded[i].ingredient, transferedAmount);

		RemoveIngredientAmount(ingredientsAdded[i].ingredient, transferedAmount);
	}
}

float UIngredientsContainerComponent::GetTotalQuantity(EIngredientType ingredientFilter)
{
	float totalQuantity = 0;
	for (int i = 0; i < ingredientsAdded.Num(); ++i)
	{
		switch (ingredientFilter)
		{
		case EIngredientType::ANY:
		{
			totalQuantity += ingredientsAdded[i].quantity;
			break;
		}
		case EIngredientType::LIQUID:
		{
			ULiquidIngredientData* liquidFilter = Cast<ULiquidIngredientData>(ingredientsAdded[i].ingredient);
			if (liquidFilter)
				totalQuantity += ingredientsAdded[i].quantity;
			break;
		}
		case EIngredientType::SOLID:
		{
			USolidIngredientData* solidFilter = Cast<USolidIngredientData>(ingredientsAdded[i].ingredient);
			if (solidFilter)
				totalQuantity += ingredientsAdded[i].quantity;
			break;
		}
		}
	}
	return totalQuantity;
}

int UIngredientsContainerComponent::GetNumberOfIngredients(EIngredientType ingredientFilter)
{
	int ingredientsNb = 0;
	switch (ingredientFilter)
	{
	case EIngredientType::ANY:
	{
		return ingredientsAdded.Num();
	}
	case EIngredientType::LIQUID:
	{
		for (int i = 0; i < ingredientsAdded.Num(); ++i)
		{
			ULiquidIngredientData* liquidIngredient = Cast<ULiquidIngredientData>(ingredientsAdded[i].ingredient);
			if (liquidIngredient)
				++ingredientsNb;
		}
		break;
	}
	case EIngredientType::SOLID:
	{
		for (int i = 0; i < ingredientsAdded.Num(); ++i)
		{
			USolidIngredientData* solidIngredient = Cast<USolidIngredientData>(ingredientsAdded[i].ingredient);
			if (solidIngredient)
				++ingredientsNb;
		}
		break;
	}
	}
	return ingredientsNb;
}

void UIngredientsContainerComponent::AcceptAllRecipes()
{
	if (allRecipes.Num() == 0)
		FetchAllPossibleRecipes();

	for (int i = 0; i < allRecipes.Num(); ++i)
		acceptableRecipes.AddUnique(allRecipes[i]);
}

void UIngredientsContainerComponent::RunMixSimulation(float temp, float pressure, float timeStep)
{
	float currentMl = -1;
	float currentG = -1;
	ULiquidContainerComp* liquidComp = Utility::GetComponentUtility<ULiquidContainerComp>(GetOwner());
	if (liquidComp)
		currentMl = liquidComp->GetCurrentMl();
	USolidExchangeComponentBase* solidComponent = Utility::GetComponentUtility<USolidExchangeComponentBase>(GetOwner());
	if (solidComponent)
		currentG = solidComponent->weight;
	if (currentG == -1 || currentMl == -1 || !IsAnyRecipeMade())
		return;

	ULiquidIngredientData* liquidIngredient;
	USolidIngredientData* solidIngredient = Cast<USolidIngredientData>(completedRecipes->ingredients[0]);
	if (solidIngredient)
	{
		liquidIngredient = Cast<ULiquidIngredientData>(completedRecipes->ingredients[1]);
	}
	else
	{
		liquidIngredient = Cast<ULiquidIngredientData>(completedRecipes->ingredients[0]);
		solidIngredient = Cast<USolidIngredientData>(completedRecipes->ingredients[1]);
	}
	if (solidIngredient && liquidIngredient)
	{
		completedRecipes->GetMixCalculator()->runSimulationByStep(
			solidIngredient->surface, solidIngredient->molarMass, completedRecipes->prefactor,
			completedRecipes->activationEnergy, completedRecipes->enthalpy, 0, 0.1, temp,
			liquidIngredient->Concentration, currentG, pressure, currentMl, 0.01, timeStep);
	}
}

void UIngredientsContainerComponent::PasteRecipes()
{
	if(copiedRecipeArray.Num() > 0)
		acceptableRecipes = copiedRecipeArray;
}

void UIngredientsContainerComponent::CopyRecipes()
{
	copiedRecipeArray = acceptableRecipes;
}

void UIngredientsContainerComponent::ClearIngredients()
{
	if(IsRunningDedicatedServer() || GetOwner()->HasNetOwner())
		S_ClearAllIngredientsAdded();
}

void UIngredientsContainerComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UIngredientsContainerComponent, ingredientsAdded);
	DOREPLIFETIME(UIngredientsContainerComponent, completedRecipes);
}

void UIngredientsContainerComponent::S_SetIngredientAdded_Implementation(FRuntimeIngredientData newIngredient)
{
	if(!HasIngredient(newIngredient.ingredient->uniqueName))
	{
		ingredientsAdded.Add(newIngredient);
		LookIfRecipeWasMade(newIngredient.ingredient);
	}
	else
	{
		ingredientsAdded[GetRuntimeIngredientIndex(newIngredient.ingredient)].quantity += newIngredient.quantity;
	}
	ULiquidIngredientData* liquidIngredient = Cast<ULiquidIngredientData>(newIngredient.ingredient);
	ULiquidContainerComp* liquidComp = Utility::GetComponentUtility<ULiquidContainerComp>(GetOwner());
	if (liquidIngredient && liquidComp)
	{
		float totalLiquidQuantity = GetTotalQuantity(EIngredientType::LIQUID);
		if (liquidComp->maxMl < totalLiquidQuantity)
			ingredientsAdded[GetRuntimeIngredientIndex(newIngredient.ingredient)].quantity -= totalLiquidQuantity - liquidComp->maxMl;

	}
}

void UIngredientsContainerComponent::S_RemoveIngredient_Implementation(UIngredientData* ingredient, float quantity)
{
	int index = GetRuntimeIngredientIndex(ingredient);
	if (index == -1)
		return;
	
	if (quantity >= ingredientsAdded[index].quantity)
		ingredientsAdded.RemoveAt(index);
	else
		ingredientsAdded[index].quantity -= quantity;
}

void UIngredientsContainerComponent::S_ClearAllIngredientsAdded_Implementation()
{
	ingredientsAdded.Empty();
	completedRecipes = nullptr;
}