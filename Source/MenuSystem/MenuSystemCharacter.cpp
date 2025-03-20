// Copyright Epic Games, Inc. All Rights Reserved.

#include "MenuSystemCharacter.h"
#include "Engine/LocalPlayer.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/Controller.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemUtils.h"

#include "OnlineSessionSettings.h"
#include "Online/OnlineSessionNames.h"

DEFINE_LOG_CATEGORY(LogTemplateCharacter);

//////////////////////////////////////////////////////////////////////////
// AMenuSystemCharacter

AMenuSystemCharacter::AMenuSystemCharacter():
	CreateSessionCompleteDelegate(FOnCreateSessionCompleteDelegate::CreateUObject(this, &ThisClass::OnCreatesessionComplete)),
	FindSessionsCompleteDelegate(FOnFindSessionsCompleteDelegate::CreateUObject(this, &ThisClass::OnFindSessionsComplete)),
	JoinSessionCompleteDelegate(FOnJoinSessionCompleteDelegate::CreateUObject(this, &ThisClass::OnJoinSessionComplete))
{
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);
		
	// Don't rotate when the controller rotates. Let that just affect the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = true; // Character moves in the direction of input...	
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f); // ...at this rotation rate

	// Note: For faster iteration times these variables, and many more, can be tweaked in the Character Blueprint
	// instead of recompiling to adjust them
	GetCharacterMovement()->JumpZVelocity = 700.f;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = 500.f;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;

	// Create a camera boom (pulls in towards the player if there is a collision)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f; // The camera follows at this distance behind the character	
	CameraBoom->bUsePawnControlRotation = true; // Rotate the arm based on the controller

	// Create a follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName); // Attach the camera to the end of the boom and let the boom adjust to match the controller orientation
	FollowCamera->bUsePawnControlRotation = false; // Camera does not rotate relative to arm

	// Note: The skeletal mesh and anim blueprint references on the Mesh component (inherited from Character) 
	// are set in the derived blueprint asset named ThirdPersonCharacter (to avoid direct content references in C++)
	if (const IOnlineSubsystem* OnlineSubsystem = Online::GetSubsystem(GetWorld()))
	{
		OnlineSessionPtr = OnlineSubsystem->GetSessionInterface();
		// if (GEngine)
		// {
		// 	GEngine->AddOnScreenDebugMessage(
		// 		-1,
		// 		3.f,
		// 		FColor::Red,
		// 		FString::Printf(TEXT("Found:%s"), *OnlineSubsystem->GetSubsystemName().ToString()));
		// }
	}
}

//////////////////////////////////////////////////////////////////////////
// Input

void AMenuSystemCharacter::NotifyControllerChanged()
{
	Super::NotifyControllerChanged();

	// Add Input Mapping Context
	if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
		{
			Subsystem->AddMappingContext(DefaultMappingContext, 0);
		}
	}
}

void AMenuSystemCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent)) {
		
		// Jumping
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);

		// Moving
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AMenuSystemCharacter::Move);

		// Looking
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AMenuSystemCharacter::Look);
	}
	else
	{
		UE_LOG(LogTemplateCharacter, Error, TEXT("'%s' Failed to find an Enhanced Input component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."), *GetNameSafe(this));
	}
}

void AMenuSystemCharacter::CreateGameSession()
{
	if (!OnlineSessionPtr.IsValid()) return;

	auto ExistingSession = OnlineSessionPtr->GetNamedSession(NAME_GameSession);
	if (ExistingSession != nullptr)
	{
		OnlineSessionPtr->DestroySession(NAME_GameSession);
	}
	
	OnlineSessionPtr->AddOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteDelegate);

	TSharedPtr<FOnlineSessionSettings> SessionSettings = MakeShareable(new FOnlineSessionSettings());
	SessionSettings->bIsLANMatch = false; // 使用局域网
	SessionSettings->NumPublicConnections = 4; // 最大连接数
	SessionSettings->bAllowJoinInProgress = true; // 是否允许中途加入
	SessionSettings->bAllowJoinViaPresence = true; // 允许区域玩家加入
	SessionSettings->bShouldAdvertise = true; // 是否被广播，可以让其他玩家发现并加入
	SessionSettings->bUsesPresence = true; // 显示用户状态信息
	SessionSettings->bUseLobbiesIfAvailable = true; // 支持 Lobbies Api，不开启可能无法找到 Session
	// 设置 MatchType 为 "FreeForAll"
	SessionSettings->Set(FName("MatchType"), FString("FreeForAll"), EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	
	const ULocalPlayer* LocalPlayer = GetWorld()->GetFirstLocalPlayerFromController();
	OnlineSessionPtr->CreateSession(*LocalPlayer->GetPreferredUniqueNetId(), NAME_GameSession, *SessionSettings);
	
}

void AMenuSystemCharacter::JoinGameSession()
{
	// Find game sessions
	if (!OnlineSessionPtr.IsValid()) {
		return;
	}

	// 将委托添加到 FindSessionsCompleteDelegate_Handle 委托列表
	// 找到 Session 后执行 FindSessionCompleteDelege 绑定的函数 OnFindSessionsComplete
	OnlineSessionPtr->AddOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegate);

	SessionSearch = MakeShareable(new FOnlineSessionSearch()); // 创建 SessionSearch 对象
	SessionSearch->MaxSearchResults = 10000; // 最大搜索结果为 10000 条
	SessionSearch->bIsLanQuery = false; // 关闭局域网查询
	SessionSearch->QuerySettings.Set(SEARCH_LOBBIES, false, EOnlineComparisonOp::Equals); // 只查询 presence 值为 false 的

	// 获取玩家控制器，以供后面获取网络 ID
	const ULocalPlayer* LocalPlayer = GetWorld()->GetFirstLocalPlayerFromController();
	// 通过 网络ID、SessionSearch 搜索参数 来查找 Session
	OnlineSessionPtr->FindSessions(*LocalPlayer->GetPreferredUniqueNetId(), SessionSearch.ToSharedRef());

}

void AMenuSystemCharacter::OnFindSessionsComplete(bool bWasSuccessful)
{
	if (!OnlineSessionPtr.IsValid()) {
		return;
	}
	if (bWasSuccessful)
	{
		// 打印搜索结果数量
		GEngine->AddOnScreenDebugMessage(
			-1,
			15.f,
			FColor::Cyan,
			FString::Printf(TEXT("Found SearchResults %d"), SessionSearch->SearchResults.Num())
		);
		// 遍历结果，取出每条结果的 Id、User，打印它们
		for (auto Result : SessionSearch->SearchResults) {
			FString Id = Result.GetSessionIdStr();
			FString User = Result.Session.OwningUserName;

			// 获取 MatchType
			FString MatchType;
			Result.Session.SessionSettings.Get(FName("MatchType"), MatchType);

			
			if (GEngine) {
				GEngine->AddOnScreenDebugMessage(
					-1,
					15.f,
					FColor::Cyan,
					FString::Printf(TEXT("Id: %s, User: %s"), *Id, *User)
				);
			}

			// 如果 MatchType 是前面 CreateGameSeesion 中设置的 "FreeForAll"，则打印并添加委托，同时加入 Session
			if (MatchType == FString("FreeForAll")) {
				if (GEngine) {
					GEngine->AddOnScreenDebugMessage(
						-1,
						15.f,
						FColor::Cyan,
						FString::Printf(TEXT("Joining Match Type %s"), *MatchType)
					);
				}

				OnlineSessionPtr->AddOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegate);

				const ULocalPlayer* LocalPlayer = GetWorld()->GetFirstLocalPlayerFromController();
				OnlineSessionPtr->JoinSession(*LocalPlayer->GetPreferredUniqueNetId(), NAME_GameSession, Result);
			}

		}
	}
}

void AMenuSystemCharacter::OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
	if (!OnlineSessionPtr.IsValid()) {
		return;
	}
	if (EOnJoinSessionCompleteResult::Success == Result)
	{
		FString Address;
		if (OnlineSessionPtr->GetResolvedConnectString(NAME_GameSession, Address))
		{
			if (GEngine) {
				GEngine->AddOnScreenDebugMessage(
					-1,
					15.f,
					FColor::Yellow,
					FString::Printf(TEXT("Connect string:%s"), *Address)
				);
			}
			APlayerController* PlayerController = GetGameInstance()->GetFirstLocalPlayerController();
			if (PlayerController)
			{
				PlayerController->ClientTravel(Address, TRAVEL_Absolute);
			}
		}
	}
}

void AMenuSystemCharacter::OnCreatesessionComplete(FName SessionName, bool bWasSuccessful)
{
	if (bWasSuccessful)
	{
		GEngine->AddOnScreenDebugMessage(
				-1,
				3.f,
				FColor::Blue,
				FString::Printf(TEXT("OnCreatesessionComplete:%s"), *SessionName.ToString()));
		// 切换到名为 Lobby 的关卡
		UWorld* World = GetWorld();
		if (World) {
			World->ServerTravel(FString("/Game/ThirdPerson/Maps/Lobby?listen"));
		}
	} else
	{
		GEngine->AddOnScreenDebugMessage(
				-1,
				3.f,
				FColor::Green,
				FString::Printf(TEXT("OnCreatesessionComplete-failed")));
	}
}

void AMenuSystemCharacter::Move(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D MovementVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		// find out which way is forward
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// get forward vector
		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	
		// get right vector 
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		// add movement 
		AddMovementInput(ForwardDirection, MovementVector.Y);
		AddMovementInput(RightDirection, MovementVector.X);
	}
}

void AMenuSystemCharacter::Look(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		// add yaw and pitch input to controller
		AddControllerYawInput(LookAxisVector.X);
		AddControllerPitchInput(LookAxisVector.Y);
	}
}
