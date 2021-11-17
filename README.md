# ProteusVR

Some classes I made with ProteusVR.
IngredientsContainerComponent is a simple recipe and ingredients system. It is a component you would add to an actor to save the indredients it has and to know when a certain recipe is made.
LiquidContainerComp is another component so an actor could have liquid inside of it. The liquid is visible so it changes depending on the amount and the nature of the liquid. It does some other stuff like detecting if the liquid container is dripping depending on the liquid height and orientation for example. Since it can make the liquid drip inside another container it controls IngredientContainerComponent to tell him which ingredient just got added to the container.
All the Pipettes classes are another component that can control IngredientContainerComponent. They are all a child of the base Pipette class and it was made to add or remove to and from a liquid container.
