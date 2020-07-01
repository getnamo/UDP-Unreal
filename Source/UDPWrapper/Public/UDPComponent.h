#pragma once

#include "Components/ActorComponent.h"
#include "Sockets/Public/IPAddress.h"
#include "Common/UdpSocketBuilder.h"
#include "Common/UdpSocketReceiver.h"
#include "Common/UdpSocketSender.h"
#include "UDPComponent.generated.h"

USTRUCT(BlueprintType)
struct UDPWRAPPER_API FUDPSettings
{
	GENERATED_USTRUCT_BODY()

	/** Default sending socket IP string in form e.g. 127.0.0.1. */
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

	/** If true will auto-connect on begin play to IP/port specified for sending udp messages, plus when emit is called */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UDP Connection Properties")
	bool bShouldAutoOpenSend;

	/** If true will auto-listen on begin play to port specified for receiving udp messages. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UDP Connection Properties")
	bool bShouldAutoOpenReceive;

	/** Whether we should process our data on the gamethread or the udp thread. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UDP Connection Properties")
	bool bReceiveDataOnGameThread;

	UPROPERTY(BlueprintReadOnly, Category = "UDP Connection Properties")
	bool bIsReceiveOpen;

	UPROPERTY(BlueprintReadOnly, Category = "UDP Connection Properties")
	bool bIsSendOpen;

	FUDPSettings();
};

class UDPWRAPPER_API FUDPNative
{
public:

	TFunction<void(const TArray<uint8>&)> OnReceivedBytes;
	TFunction<void(int32 Port)> OnReceiveOpened;
	TFunction<void(int32 Port)> OnReceiveClosed;
	TFunction<void(int32 Port)> OnSendOpened;
	TFunction<void(int32 Port)> OnSendClosed;

	FUDPSettings Settings;

	FUDPNative();
	~FUDPNative();

	//Send
	void OpenSendSocket(const FString& InIP = TEXT("127.0.0.1"), const int32 InPort = 3000);
	void CloseSendSocket();

	void EmitBytes(const TArray<uint8>& Bytes);

	//Receive
	void OpenReceiveSocket(const int32 InListenPort = 3002);
	void CloseReceiveSocket();

	//Callback convenience
	void ClearSendCallbacks();
	void ClearReceiveCallbacks();
protected:

	FSocket* SenderSocket;
	FSocket* ReceiverSocket;
	FUdpSocketReceiver* UDPReceiver;
	FString SocketDescription;
	TSharedPtr<FInternetAddr> RemoteAdress;
	ISocketSubsystem* SocketSubsystem;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FUDPSocketStateSignature, int32, Port);
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

	/** Callback when we start listening on the udp receive socket*/
	UPROPERTY(BlueprintAssignable, Category = "UDP Events")
	FUDPSocketStateSignature OnReceiveSocketOpened;

	/** Called after receiving socket has been closed. */
	UPROPERTY(BlueprintAssignable, Category = "UDP Events")
	FUDPSocketStateSignature OnReceiveSocketClosed;

	/** The send pipeline is ready to use */
	UPROPERTY(BlueprintAssignable, Category = "UDP Events")
	FUDPSocketStateSignature OnSendSocketOpened;

	/** The send pipeline can't receive emit */
	UPROPERTY(BlueprintAssignable, Category = "UDP Events")
	FUDPSocketStateSignature OnSendSocketClosed;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UDP Connection Properties")
	FUDPSettings Settings;

	/**
	* Connect to a udp endpoint, optional method if auto-connect is set to true.
	* Emit function will then work as long the network is reachable. By default
	* it will attempt this setup for this socket on beginplay.
	*
	* @param InIP the ip4 you wish to connect to
	* @param InPort the udp port you wish to connect to
	*/
	UFUNCTION(BlueprintCallable, Category = "UDP Functions")
	void OpenSendSocket(const FString& InIP = TEXT("127.0.0.1"), const int32 InPort = 3000);

	/**
	* Close the sending socket. This is usually automatically done on endplay.
	*/
	UFUNCTION(BlueprintCallable, Category = "UDP Functions")
	void CloseSendSocket();

	/** 
	* Start listening at given port for udp messages. Will auto-listen on begin play by default
	*/
	UFUNCTION(BlueprintCallable, Category = "UDP Functions")
	void OpenReceiveSocket(const int32 InListenPort = 3002);

	/**
	* Close the receiving socket. This is usually automatically done on endplay.
	*/
	UFUNCTION(BlueprintCallable, Category = "UDP Functions")
	void CloseReceiveSocket();

	/**
	* Emit specified bytes to the udp channel.
	*
	* @param Message	Bytes
	*/
	UFUNCTION(BlueprintCallable, Category = "UDP Functions")
	void EmitBytes(const TArray<uint8>& Bytes);

	virtual void InitializeComponent() override;
	virtual void UninitializeComponent() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	
protected:
	TSharedPtr<FUDPNative> Native;
	void LinkupCallbacks();
};