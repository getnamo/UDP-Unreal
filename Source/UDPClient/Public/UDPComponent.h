#pragma once

#include "Components/ActorComponent.h"
#include "Networking.h"
#include "UDPComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FUDPEventSignature);

UCLASS(ClassGroup = "Networking", meta = (BlueprintSpawnableComponent))
class UDPCLIENT_API UUDPComponent : public UActorComponent
{
	GENERATED_UCLASS_BODY()
public:

	//Async events

	/** On bound event received. */
	UPROPERTY(BlueprintAssignable, Category = "UDP Events")
	FUDPEventSignature OnEvent;

	/** Received on socket.io connection established. */
	UPROPERTY(BlueprintAssignable, Category = "UDP Events")
	FUDPEventSignature OnConnected;

	/** 
	* Received on socket.io connection disconnected. This may never get 
	* called in default settings, see OnConnectionProblems event for details. 
	*/
	UPROPERTY(BlueprintAssignable, Category = "UDP Events")
	FUDPEventSignature OnDisconnected;



	/** Default connection IP string in form e.g. 127.0.0.1. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UDP Connection Properties")
	FString IP;

	/** Default connection port e.g. 3000*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UDP Connection Properties")
	int32 Port;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UDP Connection Properties")
	FString SocketName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UDP Connection Properties")
	int32 BufferSize;

	/** If true will auto-connect on begin play to address specified in AddressAndPort. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UDP Connection Properties")
	bool bShouldAutoConnect;

	/** Delay between reconnection attempts */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UDP Connection Properties")
	int32 ReconnectionDelayInMs;

	/**
	* Number of times the connection should try before giving up.
	* Default: infinity, this means you never truly disconnect, just suffer connection problems 
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UDP Connection Properties")
	int32 MaxReconnectionAttempts;

	/** Optional parameter to limit reconnections by elapsed time. Default: infinity. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UDP Connection Properties")
	float ReconnectionTimeout;


	/** 
	* Toggle which enables plugin scoped connections. 
	* If you enable this the connection will remain until you manually call disconnect
	* or close the app. The latest connection with the same PluginScopedId will use the same connection
	* as the previous one and receive the same events.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UDP Scope Properties")
	bool bPluginScopedConnection;

	/** If you leave this as is all plugin scoped connection components will share same connection*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UDP Scope Properties")
	FString PluginScopedId;

	UPROPERTY(BlueprintReadOnly, Category = "UDP Connection Properties")
	bool bIsConnected;

	/** When connected this session id will be valid and contain a unique Id. */
	UPROPERTY(BlueprintReadOnly, Category = "UDP Connection Properties")
	FString SessionId;

	UPROPERTY(BlueprintReadOnly, Category = "UDP Connection Properties")
	bool bIsHavingConnectionProblems;

	/**
	* Connect to a udp server, optional method if auto-connect is set to true.
	*
	* @param AddressAndPort	the address in URL format with port
	*
	*/
	UFUNCTION(BlueprintCallable, Category = "UDP Functions")
	void Connect(const FString& InIP = TEXT("127.0.0.1"), const int32 InPort = 3000);

	/**
	* Disconnect from current socket.io server. This is an asynchronous action,
	* subscribe to OnDisconnected to know when you can safely continue from a 
	* disconnected state.
	*
	* @param AddressAndPort	the address in URL format with port
	*/
	UFUNCTION(BlueprintCallable, Category = "UDP Functions")
	void Disconnect();

	//
	//Blueprint Functions
	//

	/**
	* Emit bytes to the udp channel.
	*
	* @param Message	Bytes
	*/
	UFUNCTION(BlueprintCallable, Category = "UDP Functions")
	void Emit(const TArray<uint8>& Bytes);

	virtual void InitializeComponent() override;
	virtual void UninitializeComponent() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	
protected:
	FSocket* SenderSocket;
	FString SocketDescription;
	TSharedPtr<FInternetAddr> RemoteAdress;
	ISocketSubsystem* SocketSubsystem;
};