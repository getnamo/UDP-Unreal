#pragma once

#include "Components/ActorComponent.h"
#include "Networking.h"
#include "Runtime/Sockets/Public/IPAddress.h"
#include "UDPComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FUDPEventSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FUDPMessageSignature, const TArray<uint8>&, Bytes);

UCLASS(ClassGroup = "Networking", meta = (BlueprintSpawnableComponent))
class UDPCLIENT_API UUDPComponent : public UActorComponent
{
	GENERATED_UCLASS_BODY()
public:

	//Async events

	/** On message received on the receiving socket. */
	UPROPERTY(BlueprintAssignable, Category = "UDP Events")
	FUDPMessageSignature OnMessage;

	/** Received when a udp connection should be connected for sending messages, no guarantee this has happened. */
	UPROPERTY(BlueprintAssignable, Category = "UDP Events")
	FUDPEventSignature OnSendSocketConnected;

	/** If we failed to connect for whatever reason. */
	UPROPERTY(BlueprintAssignable, Category = "UDP Events")
	FUDPEventSignature OnSendSocketConnectionProblem;

	/** Without a custom system on both ends, we can only assume when udp is disconnected. */
	UPROPERTY(BlueprintAssignable, Category = "UDP Events")
	FUDPEventSignature OnSendSocketDisconnected;

	/** Callback when we start listening */
	UPROPERTY(BlueprintAssignable, Category = "UDP Events")
	FUDPEventSignature OnReceiveSocketStarted;

	/** Called after receiving socket has been closed. */
	UPROPERTY(BlueprintAssignable, Category = "UDP Events")
	FUDPEventSignature OnReceiveSocketClosed;


	/** Default connection IP string in form e.g. 127.0.0.1. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UDP Connection Properties")
	FString SendIP;

	/** Default connection port e.g. 3001*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UDP Connection Properties")
	int32 SendPort;

	/** Default connection port e.g. 3002*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UDP Connection Properties")
	int32 ReceivePort;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UDP Connection Properties")
	FString SendSocketName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UDP Connection Properties")
	FString ReceiveSocketName;

	/** in bytes */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UDP Connection Properties")
	int32 BufferSize;

	/** If true will auto-connect on begin play to IP/port specified for sending udp messages. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UDP Connection Properties")
	bool bShouldAutoConnect;

	/** If true will auto-listen on begin play to port specified for receiving udp messages. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UDP Connection Properties")
	bool bShouldAutoListen;

	UPROPERTY(BlueprintReadOnly, Category = "UDP Connection Properties")
	bool bIsConnected;

	/** When connected this session id will be valid and contain a unique Id. */
	UPROPERTY(BlueprintReadOnly, Category = "UDP Connection Properties")
	FString SessionId;

	UPROPERTY(BlueprintReadOnly, Category = "UDP Connection Properties")
	bool bIsHavingConnectionProblems;

	/**
	* Connect to a udp endpoint, optional method if auto-connect is set to true.
	*
	* @param AddressAndPort	the address in URL format with port
	*
	*/
	UFUNCTION(BlueprintCallable, Category = "UDP Functions")
	void ConnectToSendSocket(const FString& InIP = TEXT("127.0.0.1"), const int32 InPort = 3000);

	/** 
	* Start listening at given port for udp messages. Will auto-listen on begin play by default
	*/
	UFUNCTION(BlueprintCallable, Category = "UDP Functions")
	void StartReceiveSocket(const int32 InListenPort = 3002);

	UFUNCTION(BlueprintCallable, Category = "UDP Functions")
	void CloseReceiveSocket();

	/**
	* Disconnect from current socket.io server. This is an asynchronous action,
	* subscribe to OnDisconnected to know when you can safely continue from a 
	* disconnected state.
	*
	* @param AddressAndPort	the address in URL format with port
	*/
	UFUNCTION(BlueprintCallable, Category = "UDP Functions")
	void CloseSendSocket();

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
	FSocket* ReceiverSocket;

	FUdpSocketReceiver* UDPReceiver;
	FString SocketDescription;
	TSharedPtr<FInternetAddr> RemoteAdress;
	ISocketSubsystem* SocketSubsystem;
};