// Copyright (c) 2017 Bytecode Bits

#include "ZynapsReloaded.h"
#include "PlayerPawn.h"

// Log category
DEFINE_LOG_CATEGORY(LogPlayerPawn);

// Sets default values
APlayerPawn::APlayerPawn() : Super()
{
 	// Set this pawn to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	// Set up root component
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));

	// Set up the collision capsule
	CapsuleComponent = CreateCapsuleComponent(RootComponent);

	// Set up the mesh component
	MeshComponent = CreateMeshComponent(CapsuleComponent);

	// Set up the engine thrust particle system
	EngineThrustSocketName = TEXT("EngineThrust");
	EnginePartSystemComponent = CreateEngineThrustParticleSystem(MeshComponent, EngineThrustSocketName);

	// Set up the Projector 2D component
	Projector2DComponent = CreateProjector2DComponent();

	// Sets this pawn to be controlled by the lowest-numbered player
	AutoPossessPlayer = EAutoReceiveInput::Player0;

	// Inits player state
	AZynapsPlayerState* State = GetZynapsPlayerState();
	MovementSpeed = 2000.0f;
	if (State)
	{
		State->Lives = 3;
		State->SpeedUpLevel = 0;
		State->LaserPower = 0;
		State->PlasmaBombs = false;
		State->HomingMissiles = false;
		State->SeekerMissiles = false;
	}

	// Init shooting vars
	ProjectileClass = APlayerProjectile::StaticClass();
	LeftCannonSocketName = TEXT("LeftCannon");
	RightCannonSocketName = TEXT("RightCannon");
	TopCannonSocketName = TEXT("TopCannon");
	NextCannon = RightCannon;
}

// Creates the capsule component used for collision detection
UCapsuleComponent* APlayerPawn::CreateCapsuleComponent(USceneComponent* Parent)
{
	UCapsuleComponent* Component = CreateDefaultSubobject<UCapsuleComponent>(TEXT("CapsuleComponent"));
	Component->InitCapsuleSize(18.0f, 32.0f);
	Component->SetWorldLocation(FVector(0.0f, 0.0f, 0.0f));
	Component->SetWorldRotation(FRotator(0.0f, 180.0f, -90.0f));
	Component->SetWorldScale3D(FVector(12.0f, 10.0f, 10.0f));
	Component->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Component->bGenerateOverlapEvents = true;
	Component->SetNotifyRigidBodyCollision(true);
	Component->SetCollisionObjectType(ECollisionChannel::ECC_Pawn);
	Component->SetCanEverAffectNavigation(false);
	Component->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Ignore);
	Component->SetCollisionResponseToChannel(ECollisionChannel::ECC_WorldStatic, ECollisionResponse::ECR_Block);
	Component->SetCollisionResponseToChannel(ECollisionChannel::ECC_WorldDynamic, ECollisionResponse::ECR_Block);
	Component->SetSimulatePhysics(true);
	Component->SetEnableGravity(false);
	Component->SetAngularDamping(0.0f);
	Component->SetLinearDamping(0.0f);
	Component->SetConstraintMode(EDOFMode::YZPlane);
	Component->BodyInstance.bLockXRotation = true;
	Component->BodyInstance.bLockYRotation = true;
	Component->BodyInstance.bLockZRotation = true;
	Component->BodyInstance.bLockXTranslation = true;
	//Component->SetHiddenInGame(false);
	Component->WakeRigidBody();
	Component->OnComponentBeginOverlap.AddDynamic(this, &APlayerPawn::BeginOverlap);
	Component->OnComponentEndOverlap.AddDynamic(this, &APlayerPawn::EndOverlap);
	Component->OnComponentHit.AddDynamic(this, &APlayerPawn::Hit);
	Component->SetupAttachment(Parent);

	return Component;
}

// Creates the mesh component which models the ship
UStaticMeshComponent* APlayerPawn::CreateMeshComponent(USceneComponent* Parent)
{
	UStaticMeshComponent* Component = CreateDefaultSubobject<UStaticMeshComponent>(
		TEXT("MeshComponent"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> MeshObj(
		TEXT("StaticMesh'/Game/Models/Ship/zynaps_ship.zynaps_ship'"));
	if (MeshObj.Succeeded())
	{
		Component->SetStaticMesh(MeshObj.Object);
		Component->SetRelativeLocation(FVector(0.0f, -3.5f, -7.0f));
		Component->SetRelativeRotation(FRotator(0.0f, 0.0f, 90.0f));
		Component->SetRelativeScale3D(FVector(1.0f / 12.0f, 0.1f, 0.1f));
		Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Component->SetSimulatePhysics(false);
		Component->SetupAttachment(Parent);
	}
	else
	{
		UE_LOG(LogPlayerPawn, Error, 
			TEXT("The asset StaticMesh'/Game/Models/Ship/zynaps_ship.zynaps_ship' was not found"));
		return nullptr;
	}

	return Component;
}

// Creates the particle system for the engine thrust
UParticleSystemComponent* APlayerPawn::CreateEngineThrustParticleSystem(USceneComponent* Parent, FName SocketName)
{
	UParticleSystemComponent* Component = 
		CreateDefaultSubobject<UParticleSystemComponent>(TEXT("EnginePartSystemComponent"));
	static ConstructorHelpers::FObjectFinder<UParticleSystem> PartSystem(
		TEXT("ParticleSystem'/Game/Models/ThrustParticle/EngineThrustSystem.EngineThrustSystem'"));
	if (PartSystem.Succeeded())
	{
		Component->SetTemplate(PartSystem.Object);
		Component->SetRelativeScale3D(FVector(1.5f, 0.5f, 1.5f));
		Component->bAutoActivate = true;
		Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Component->SetSimulatePhysics(false);
		Component->SetupAttachment(Parent, SocketName);
	}
	else
	{
		UE_LOG(LogPlayerPawn, Error,
			TEXT("The asset ParticleSystem'/Game/Models/ThrustParticle/EngineThrustSystem.EngineThrustSystem' was not found"));
		return nullptr;
	}

	return Component;
}

// Creates the Projector 2D component
UProjector2DComponent* APlayerPawn::CreateProjector2DComponent()
{
	UProjector2DComponent* Component = CreateDefaultSubobject<UProjector2DComponent>(TEXT("Projector2DComponent"));

	return Component;
}

// Called when the game starts or when spawned
void APlayerPawn::BeginPlay()
{
	Super::BeginPlay();
}

// Called every frame
void APlayerPawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// This should be done by means of a proper PlayerCameraManager
	CapsuleComponent->AddWorldOffset(FVector(0.0f, 1000.0f * DeltaSeconds, 0.0f));
}

// Returns a reference to the instance of AZynapsPlayerState or NULL if it doesn't exist
AZynapsPlayerState* APlayerPawn::GetZynapsPlayerState() const
{
	AZynapsPlayerState* Result = NULL;
	if (PlayerState)
	{
		Result = Cast<AZynapsPlayerState>(PlayerState);
	}
	return Result;
}

// Called to move the player up
void APlayerPawn::MoveUp(float Val)
{
	// Get the speed-up level
	uint8 SpeedUpLevel = 0;
	AZynapsPlayerState* State = GetZynapsPlayerState();
	if (State) 
	{
		SpeedUpLevel = State->SpeedUpLevel;
	}

	// Calculate the next position
	float Offset = (MovementSpeed + (MovementSpeed * SpeedUpLevel / 10)) * GetWorld()->GetDeltaSeconds();
	FVector NextLocation = CapsuleComponent->GetComponentLocation();
	NextLocation.Z += Offset;
	FVector2D NextPosition = Projector2DComponent->ConvertToScreenCoordinates(NextLocation);

	// Check that the movement is allowed
	FVector2D PlayerSize = Projector2DComponent->GetSizeInScreenCoordinates();
	if (NextPosition.Y - PlayerSize.Y / 2 > LimitMarginUp)
	{
		// Move the player
		CapsuleComponent->SetWorldLocation(NextLocation);
	}
}

// Called to move the player down
void APlayerPawn::MoveDown(float Val)
{
	// Get the speed-up level
	uint8 SpeedUpLevel = 0;
	AZynapsPlayerState* State = GetZynapsPlayerState();
	if (State)
	{
		SpeedUpLevel = State->SpeedUpLevel;
	}

	// Calculate the next position
	float Offset = -(MovementSpeed + (MovementSpeed * SpeedUpLevel / 10)) * GetWorld()->GetDeltaSeconds();
	FVector NextLocation = CapsuleComponent->GetComponentLocation();
	NextLocation.Z += Offset;
	FVector2D NextPosition = Projector2DComponent->ConvertToScreenCoordinates(NextLocation);

	// Check that the movement is allowed
	FVector2D PlayerSize = Projector2DComponent->GetSizeInScreenCoordinates();
	if (NextPosition.Y + PlayerSize.Y / 2 < Projector2DComponent->GetViewportSize().Y - LimitMarginDown)
	{
		// Move the player
		CapsuleComponent->SetWorldLocation(NextLocation);
	}
}

// Called to move the player to the left
void APlayerPawn::MoveLeft(float Val)
{
	// Get the speed-up level
	uint8 SpeedUpLevel = 0;
	AZynapsPlayerState* State = GetZynapsPlayerState();
	if (State)
	{
		SpeedUpLevel = State->SpeedUpLevel;
	}

	// Calculate the next position
	float Offset = -(MovementSpeed + (MovementSpeed * SpeedUpLevel / 10)) * GetWorld()->GetDeltaSeconds();
	FVector NextLocation = CapsuleComponent->GetComponentLocation();
	NextLocation.Y += Offset;
	FVector2D NextPosition = Projector2DComponent->ConvertToScreenCoordinates(NextLocation);

	// Check that the movement is allowed
	FVector2D PlayerSize = Projector2DComponent->GetSizeInScreenCoordinates();
	if (NextPosition.X - PlayerSize.X / 2 > LimitMarginLeft)
	{
		// Move the player
		CapsuleComponent->SetWorldLocation(NextLocation);
	}
}

// Called to move the player to the right
void APlayerPawn::MoveRight(float Val)
{
	// Get the speed-up level
	uint8 SpeedUpLevel = 0;
	AZynapsPlayerState* State = GetZynapsPlayerState();
	if (State)
	{
		SpeedUpLevel = State->SpeedUpLevel;
	}

	// Calculate the next position
	float Offset = (MovementSpeed + (MovementSpeed * SpeedUpLevel / 10)) * GetWorld()->GetDeltaSeconds();
	FVector NextLocation = CapsuleComponent->GetComponentLocation();
	NextLocation.Y += Offset;
	FVector2D NextPosition = Projector2DComponent->ConvertToScreenCoordinates(NextLocation);

	// Check that the movement is allowed
	FVector2D PlayerSize = Projector2DComponent->GetSizeInScreenCoordinates();
	if (NextPosition.X + PlayerSize.X / 2 < Projector2DComponent->GetViewportSize().X - LimitMarginRight)
	{
		// Move the player
		CapsuleComponent->AddWorldOffset(FVector(0.0f, Offset, 0.0f));
	}
}

// Called to fire
void APlayerPawn::Fire()
{
	// Get the transforms for the cannons
	FTransform CannonTransforms[3] = {
		GetSocketTransform(RightCannonSocketName),
		GetSocketTransform(LeftCannonSocketName),
		GetSocketTransform(TopCannonSocketName)
	};

	// Shot the cannon
	GetWorld()->SpawnActor<APlayerProjectile>(ProjectileClass, CannonTransforms[NextCannon]);

	// Prepare the next cannon to be shot
	if (++NextCannon > TopCannon)
	{
		NextCannon = RightCannon;
	}
}

// Called when the player hits other actor
void APlayerPawn::Hit_Implementation(class UPrimitiveComponent* HitComp, class AActor* OtherActor,
	class UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult &HitResult)
{
	PRINT("APlayerPawn::Hit");

	// Ensure the player does not change angle or direction when hits something
	CapsuleComponent->BodyInstance.SetAngularVelocity(FVector(0.0f, 0.0f, 0.0f), false);
	CapsuleComponent->BodyInstance.SetLinearVelocity(FVector(0.0f, 0.0f, 0.0f), false);
	CapsuleComponent->SetWorldRotation(FRotator(0.0f, 180.0f, -90.0f));
}

// Called when the player begins overlapping with another actor
void APlayerPawn::BeginOverlap_Implementation(class UPrimitiveComponent* HitComp, class AActor* OtherActor,
	class UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep,
	const FHitResult &SweepResult)
{
	// PRINT("APlayerPawn::BeginOverlap");
}

// Called when the player ends overlapping with another actor
void APlayerPawn::EndOverlap_Implementation(class UPrimitiveComponent* HitComp, class AActor* OtherActor,
	class UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	// PRINT("APlayerPawn::EndOverlap");
}

// Returns the transform of a socket
FTransform APlayerPawn::GetSocketTransform(FName SocketName) const
{
	FTransform Result;
	if (MeshComponent->DoesSocketExist(SocketName))
	{
		Result = MeshComponent->GetSocketTransform(SocketName);
	}
	return Result;
}
