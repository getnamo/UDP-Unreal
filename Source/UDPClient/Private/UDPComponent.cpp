
#include "UDPComponent.h"
#include "LambdaRunnable.h"
#include "Runtime/Sockets/Public/SocketSubsystem.h"

UUDPComponent::UUDPComponent(const FObjectInitializer &init) : UActorComponent(init)
{
	bShouldAutoConnect = true;
	bWantsInitializeComponent = true;
	bAutoActivate = true;
	IP = FString(TEXT("127.0.0.1"));
	Port = 3000;
	SocketName = FString(TEXT("ue4-dgram"));

	ReconnectionTimeout = 0.f;
	MaxReconnectionAttempts = -1.f;
	ReconnectionDelayInMs = 5000;
	BufferSize = 2 * 1024 * 1024;	//default roughly 2mb
}

void UUDPComponent::Connect(const FString& InIP /*= TEXT("127.0.0.1")*/, const int32 InPort /*= 3000*/)
{
	RemoteAdress = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateInternetAddr();
	
	bool bIsValid;
	RemoteAdress->SetIp(*InIP, bIsValid);
	RemoteAdress->SetPort(InPort);

	if (!bIsValid)
	{
		UE_LOG(LogTemp, Error, TEXT("UDP address is invalid <%s:%d>"), *InIP, InPort);
		return ;
	}

	SenderSocket = FUdpSocketBuilder(*SocketName).AsReusable().WithBroadcast();

	//check(SenderSocket->GetSocketType() == SOCKTYPE_Datagram);

	//Set Send Buffer Size
	SenderSocket->SetSendBufferSize(BufferSize, BufferSize);
	SenderSocket->SetReceiveBufferSize(BufferSize, BufferSize);

	SenderSocket->Connect(*RemoteAdress);
	
	//todo: add correct connection event link
	OnConnected.Broadcast();
}

void UUDPComponent::Disconnect()
{
	SenderSocket->Close();

	//todo: add correct connection event link
	OnDisconnected.Broadcast();
}

void UUDPComponent::Emit(const TArray<uint8>& Bytes)
{
	if (SenderSocket->GetConnectionState() == SCS_Connected)
	{
		int32 BytesSent = 0;
		SenderSocket->Send(Bytes.GetData(), Bytes.Num(), BytesSent);
	}
}

void UUDPComponent::InitializeComponent()
{
	Super::InitializeComponent();
}

void UUDPComponent::UninitializeComponent()
{
	Super::UninitializeComponent();
}

void UUDPComponent::BeginPlay()
{
	Super::BeginPlay();

	if (bShouldAutoConnect)
	{
		Connect(IP, Port);
	}
}

void UUDPComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (SenderSocket)
	{
		SenderSocket->Close();
		ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(SenderSocket);
	}

	Super::EndPlay(EndPlayReason);
}
