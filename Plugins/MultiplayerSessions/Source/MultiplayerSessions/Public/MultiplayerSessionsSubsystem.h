// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Interfaces\OnlineSessionInterface.h"
#include "MultiplayerSessionsSubsystem.generated.h"

/**
 * Declaring our own custom delegates for the Menu class to bind callbacks to 
 **/
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMultiplayerOnSessionStateChangeComplete, bool, bWasSuccessful);
DECLARE_MULTICAST_DELEGATE_TwoParams(FMultiplayerOnFindSessionsComplete, const TArray<FOnlineSessionSearchResult>& SessionResults, bool bWasSuccessful);
DECLARE_MULTICAST_DELEGATE_OneParam(FMultiplayerOnJoinSessionComplete, EOnJoinSessionCompleteResult::Type Result);

/**
 * 
 */
UCLASS()
class MULTIPLAYERSESSIONS_API UMultiplayerSessionsSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
public:
	UMultiplayerSessionsSubsystem();

	// To handle session functionality. The Menu class will call these
	void CreateSession(int32 NumPublicConnections, FString MatchType);
	void FindSessions(int32 MaxSearchResults);
	void JoinSession(const FOnlineSessionSearchResult& SessionResult);
	void DestroySession();
	void StartSession();

	/**
	 * Our own custom delegates for the Menu class to bind callbacks to 
	 **/
	FMultiplayerOnSessionStateChangeComplete MultiplayerOnCreateSessionCompleteDelegate;
	FMultiplayerOnFindSessionsComplete MultiplayerOnFindSessionsCompleteDelegate;
	FMultiplayerOnJoinSessionComplete MultiplayerOnJoinSessionCompleteDelegate;
	FMultiplayerOnSessionStateChangeComplete MultiplayerOnDestroySessionCompleteDelegate;
	FMultiplayerOnSessionStateChangeComplete MultiplayerOnStartSessionCompleteDelegate;

	void GetResolvedConnectString(const FName& SessionName, FString& Address);
protected:
	/**
	 * Internal callbacks for the delegates we'll add to the Online Session Interface delegate list.
	 * These don't need to be called outside this class.
	 **/
	void OnCreateSessionComplete(FName SessionName, bool bWasSuccessful);
	void OnFindSessionsComplete(bool bWasSuccessful);
	void OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);
	void OnDestroySessionComplete(FName SessionName, bool bWasSuccessful);
	void OnStartSessionComplete(FName SessionName, bool bWasSuccessful);
	
private:
	IOnlineSessionPtr OnlineSessionPtr;
	TSharedPtr<FOnlineSessionSettings> LastSessionSettings;
	TSharedPtr<FOnlineSessionSearch> LastSessionSearch;

	/**
	 * To add to the Online Session Interface delegate list.
	 * We'll bind our MultiplayerSessionsSubsystem internal callbacks to these.
	 **/
	FOnCreateSessionCompleteDelegate OnCreateSessionCompleteDelegate;
	FDelegateHandle OnCreateSessionCompleteDelegateHandle;
	FOnFindSessionsCompleteDelegate OnFindSessionsCompleteDelegate;
	FDelegateHandle OnFindSessionsCompleteDelegateHandle;
	FOnJoinSessionCompleteDelegate OnJoinSessionCompleteDelegate;
	FDelegateHandle OnJoinSessionCompleteDelegateHandle;
	FOnDestroySessionCompleteDelegate OnDestroySessionCompleteDelegate;
	FDelegateHandle OnDestroySessionCompleteDelegateHandle;
	FOnStartSessionCompleteDelegate OnStartSessionCompleteDelegate;
	FDelegateHandle OnStartSessionCompleteDelegateHandle;

	bool bCreateSessionOnDestroy{false};
	int32 LastNumPublicConnections;
	FString LastMatchType;
};
