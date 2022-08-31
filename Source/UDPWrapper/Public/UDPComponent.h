#pragma once

#include "Components/ActorComponent.h"
#include "Sockets/Public/IPAddress.h"
#include "Common/UdpSocketBuilder.h"
#include "Common/UdpSocketReceiver.h"
#include "Common/UdpSocketSender.h"
#include "UDPComponent.generated.h"

//UDP Connection Settings
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

	/** Port to which send is bound to on this client (this should change on each open) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UDP Connection Properties")
	int32 SendBoundPort;

	/** IP to which send is bound to on this client (this could change on open) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UDP Connection Properties")
	FString SendBoundIP;

	/** Default receiving socket IP string in form e.g. 0.0.0.0 for all connections, may need 127.0.0.1 for some cases. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UDP Connection Properties")
	FString ReceiveIP;

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

	/** This will open it to the bound send port at specified send ip and ignore passed in settings for open receive. Default False*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UDP Connection Properties")
	bool bShouldOpenReceiveToBoundSendPort;

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

	TFunction<void(const TArray<uint8>&, const FString&, const int32&)> OnReceivedBytes;
	TFunction<void(int32 Port)> OnReceiveOpened;
	TFunction<void(int32 Port)> OnReceiveClosed;
	TFunction<void(int32 SpecifiedPort, int32 BoundPort, FString BoundIP)> OnSendOpened;
	TFunction<void(int32 Port)> OnSendClosed;

	//Default settings, on open send/receive they will sync with what was last passed into them
	FUDPSettings Settings;

	FUDPNative();
	~FUDPNative();

	//Send
	/**
	* Open socket for sending and return bound port
	*/
	int32 OpenSendSocket(const FString& InIP = TEXT("127.0.0.1"), const int32 InPort = 3000);

	/**
	* Close current sending socket, returns true if successful
	*/
	bool CloseSendSocket();

	/** 
	* Emit given bytes to send socket. If Settings.bShouldAutoOpenSend is true it will auto-open socket.
	* Returns true if bytes emitted successfully
	*/
	bool EmitBytes(const TArray<uint8>& Bytes);

	//Receive
	/**
	* Open current receiving socket, returns true if successful
	*/
	bool OpenReceiveSocket(const FString& InIP = TEXT("0.0.0.0"), const int32 InListenPort = 3002);
	/**
	* Close current receiving socket, returns true if successful
	*/
	bool CloseReceiveSocket();

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
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FUDPSocketSendStateSignature, int32, SpecifiedPort, int32, BoundPort, const FString&, BoundIP);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FUDPMessageSignature, const TArray<uint8>&, Bytes, const FString&, IPAddress, const int32&, Port);

UCLASS(ClassGroup = "Networking", meta = (BlueprintSpawnableComponent))
class UDPWRAPPER_API UUDPComponent : public UActorComponent
{
	GENERATED_UCLASS_BODY()
public:

	//Async events

	/** On message received on receive socket from Ip address */
	UPROPERTY(BlueprintAssignable, Category = "UDP Events")
	FUDPMessageSignature OnReceivedBytes;

	/** Callback when we start listening on the udp receive socket*/
	UPROPERTY(BlueprintAssignable, Category = "UDP Events")
	FUDPSocketStateSignature OnReceiveSocketOpened;

	/** Called after receiving socket has been closed. */
	UPROPERTY(BlueprintAssignable, Category = "UDP Events")
	FUDPSocketStateSignature OnReceiveSocketClosed;

	/** Called when the send socket is ready to use; optionally open your receive socket to bound send port here */
	UPROPERTY(BlueprintAssignable, Category = "UDP Events")
	FUDPSocketSendStateSignature OnSendSocketOpened;

	/** Called when the send socket has been closed */
	UPROPERTY(BlueprintAssignable, Category = "UDP Events")
	FUDPSocketStateSignature OnSendSocketClosed;

	/** Defining UDP sending and receiving Ips, ports, and other defaults*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UDP Connection Properties")
	FUDPSettings Settings;

	/**
	* Connect to a udp endpoint, optional method if auto-connect is set to true.
	* Emit function will then work as long the network is reachable. By default
	* it will attempt this setup for this socket on BeginPlay.
	*
	* @param InIP the ip4 you wish to connect to
	* @param InPort the udp port you wish to connect to
	*/
	UFUNCTION(BlueprintCallable, Category = "UDP Functions")
	int32 OpenSendSocket(const FString& InIP = TEXT("127.0.0.1"), const int32 InPort = 3000);

	/**
	* Close the sending socket. This is usually automatically done on EndPlay.
	*/
	UFUNCTION(BlueprintCallable, Category = "UDP Functions")
	bool CloseSendSocket();

	/** 
	* Start listening at given port for udp messages. Will auto-listen on BeginPlay by default. Listen IP of 0.0.0.0 means all connections.
	*/
	UFUNCTION(BlueprintCallable, Category = "UDP Functions")
	bool OpenReceiveSocket(const FString& InListenIP = TEXT("0.0.0.0"), const int32 InListenPort = 3002);

	/**
	* Close the receiving socket. This is usually automatically done on EndPlay.
	*/
	UFUNCTION(BlueprintCallable, Category = "UDP Functions")
	bool CloseReceiveSocket();

	/**
	* Emit specified bytes to the udp channel.
	*
	* @param Message	Bytes
	*/
	UFUNCTION(BlueprintCallable, Category = "UDP Functions")
	bool EmitBytes(const TArray<uint8>& Bytes);

	virtual void InitializeComponent() override;
	virtual void UninitializeComponent() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	
protected:
	TSharedPtr<FUDPNative> Native;
	void LinkupCallbacks();
};