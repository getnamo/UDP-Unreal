#pragma once

#include "Components/ActorComponent.h"
#include "Networking.h"
#include "Runtime/Sockets/Public/IPAddress.h"
#include "UDPComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FUDPEventSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FUDPMessageSignature, const TArray<uint8>&, Bytes);

UCLASS(ClassGroup = "Networking", meta = (BlueprintSpawnableComponent))
class UDPWRAPPER_API UUDPComponent : public UActorComponent
{
	GENERATED_UCLASS_BODY()
public:

	//Async events

	/** On message received on the receiving socket. */
	UPROPERTY(BlueprintAssignable, Category = "UDP Events")
	FUDPMessageSignature OnReceivedBytes;

	/** Callback when we start listening */
	UPROPERTY(BlueprintAssignable, Category = "UDP Events")
	FUDPEventSignature OnReceiveSocketStartedListening;

	/** Called after receiving socket has been closed. */
	UPROPERTY(BlueprintAssignable, Category = "UDP Events")
	FUDPEventSignature OnReceiveSocketStoppedListening;


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

	/** Whether we should process our data on the gamethread or the udp thread. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UDP Connection Properties")
	bool bReceiveDataOnGameThread;

	UPROPERTY(BlueprintReadOnly, Category = "UDP Connection Properties")
	bool bIsConnected;


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
	void StartReceiveSocketListening(const int32 InListenPort = 3002);

	UFUNCTION(BlueprintCallable, Category = "UDP Functions")
	void CloseReceiveSocket();

	/**
	* Close the sending socket
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

	void OnDataReceivedDelegate(const FArrayReaderPtr& DataPtr, const FIPv4Endpoint& Endpoint);

	FUdpSocketReceiver* UDPReceiver;
	FString SocketDescription;
	TSharedPtr<FInternetAddr> RemoteAdress;
	ISocketSubsystem* SocketSubsystem;
};