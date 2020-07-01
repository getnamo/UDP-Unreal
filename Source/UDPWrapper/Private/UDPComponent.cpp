
#include "UDPComponent.h"
#include "Async/Async.h"
#include "SocketSubsystem.h"
#include "Kismet/KismetSystemLibrary.h"

UUDPComponent::UUDPComponent(const FObjectInitializer &init) : UActorComponent(init)
{
	bWantsInitializeComponent = true;
	bAutoActivate = true;

	Native = MakeShareable(new FUDPNative);

	LinkupCallbacks();
}

void UUDPComponent::LinkupCallbacks()
{
	Native->OnSendOpened = [this](int32 Port)
	{
		OnSendSocketOpened.Broadcast(Port);
	};
	Native->OnSendClosed = [this](int32 Port)
	{
		OnSendSocketClosed.Broadcast(Port);
	};
	Native->OnReceiveOpened = [this](int32 Port)
	{
		OnReceiveSocketOpened.Broadcast(Port);
	};
	Native->OnReceiveClosed = [this](int32 Port)
	{
		OnReceiveSocketClosed.Broadcast(Port);
	};
	Native->OnReceivedBytes = [this](const TArray<uint8>& Data)
	{
		OnReceivedBytes.Broadcast(Data);
	};
}

void UUDPComponent::CloseReceiveSocket()
{
	Native->CloseReceiveSocket();
}

void UUDPComponent::OpenSendSocket(const FString& InIP /*= TEXT("127.0.0.1")*/, const int32 InPort /*= 3000*/)
{
	Native->OpenSendSocket(InIP, InPort);
}

void UUDPComponent::CloseSendSocket()
{
	Native->CloseSendSocket();
}

void UUDPComponent::OpenReceiveSocket(const int32 InListenPort /*= 3002*/)
{
	Native->OpenReceiveSocket(InListenPort);
}

void UUDPComponent::EmitBytes(const TArray<uint8>& Bytes)
{
	Native->EmitBytes(Bytes);
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
	
	//Sync settings
	Native->Settings = Settings;

	if (Settings.bShouldAutoOpenReceive)
	{
		Native->OpenReceiveSocket(Settings.ReceivePort);
	}
	if (Settings.bShouldAutoOpenSend)
	{
		Native->OpenSendSocket(Settings.SendIP, Settings.SendPort);
	}
}

void UUDPComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CloseSendSocket();
	CloseReceiveSocket();

	Native->ClearSendCallbacks();
	Native->ClearReceiveCallbacks();

	Super::EndPlay(EndPlayReason);
}

FUDPNative::FUDPNative()
{
	SenderSocket = nullptr;
	ReceiverSocket = nullptr;

	ClearReceiveCallbacks();
	ClearSendCallbacks();
}

FUDPNative::~FUDPNative()
{
	if (Settings.bIsReceiveOpen)
	{
		CloseReceiveSocket();
		ClearReceiveCallbacks();
	}
	if (Settings.bIsSendOpen)
	{
		CloseSendSocket();
		ClearSendCallbacks();
	}
}

void FUDPNative::OpenSendSocket(const FString& InIP /*= TEXT("127.0.0.1")*/, const int32 InPort /*= 3000*/)
{
	RemoteAdress = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateInternetAddr();

	bool bIsValid;
	RemoteAdress->SetIp(*InIP, bIsValid);
	RemoteAdress->SetPort(InPort);

	if (!bIsValid)
	{
		UE_LOG(LogTemp, Error, TEXT("UDP address is invalid <%s:%d>"), *InIP, InPort);
		return;
	}

	SenderSocket = FUdpSocketBuilder(*Settings.SendSocketName).AsReusable().WithBroadcast();

	//check(SenderSocket->GetSocketType() == SOCKTYPE_Datagram);

	//Set Send Buffer Size
	SenderSocket->SetSendBufferSize(Settings.BufferSize, Settings.BufferSize);
	SenderSocket->SetReceiveBufferSize(Settings.BufferSize, Settings.BufferSize);

	bool bDidConnect = SenderSocket->Connect(*RemoteAdress);
	Settings.bIsSendOpen = true;

	if (OnSendOpened)
	{	
		OnSendOpened(Settings.SendPort);
	}
}

void FUDPNative::CloseSendSocket()
{
	if (SenderSocket)
	{
		SenderSocket->Close();
		ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(SenderSocket);
		SenderSocket = nullptr;

		if (OnSendClosed)
		{
			OnSendClosed(Settings.SendPort);
		}
	}

	Settings.bIsSendOpen = false;
}

void FUDPNative::EmitBytes(const TArray<uint8>& Bytes)
{
	if (SenderSocket->GetConnectionState() == SCS_Connected)
	{
		int32 BytesSent = 0;
		SenderSocket->Send(Bytes.GetData(), Bytes.Num(), BytesSent);
	}
	else if(Settings.bShouldAutoOpenSend)
	{
		OpenSendSocket(Settings.SendIP, Settings.SendPort);
		EmitBytes(Bytes);
	}
}

void FUDPNative::OpenReceiveSocket(const int32 InListenPort /*= 3002*/)
{
	if (Settings.bIsReceiveOpen)
	{
		CloseReceiveSocket();
	}

	//(const FArrayReaderPtr& DataPtr, const FIPv4Endpoint& Endpoint);

	FIPv4Address Addr;
	FIPv4Address::Parse(TEXT("0.0.0.0"), Addr);

	//Create Socket
	FIPv4Endpoint Endpoint(Addr, InListenPort);

	ReceiverSocket = FUdpSocketBuilder(*Settings.ReceiveSocketName)
		.AsNonBlocking()
		.AsReusable()
		.BoundToEndpoint(Endpoint)
		.WithReceiveBufferSize(Settings.BufferSize);

	FTimespan ThreadWaitTime = FTimespan::FromMilliseconds(100);
	FString ThreadName = FString::Printf(TEXT("UDP RECEIVER-FUDPNative"));
	UDPReceiver = new FUdpSocketReceiver(ReceiverSocket, ThreadWaitTime, *ThreadName);

	UDPReceiver->OnDataReceived().BindLambda([this](const FArrayReaderPtr& DataPtr, const FIPv4Endpoint& Endpoint)
	{
		if (!OnReceivedBytes)
		{
			return;
		}

		TArray<uint8> Data;
		Data.AddUninitialized(DataPtr->TotalSize());
		DataPtr->Serialize(Data.GetData(), DataPtr->TotalSize());

		
		if (Settings.bReceiveDataOnGameThread)
		{
			//Pass the reference to be used on gamethread
			AsyncTask(ENamedThreads::GameThread, [this, Data]()
			{
				//double check we're still bound on this thread
				if (OnReceivedBytes)
				{
					OnReceivedBytes(Data);
				}
			});
		}
		else
		{
			OnReceivedBytes(Data);
		}
	});

	Settings.bIsReceiveOpen = true;

	if (OnReceiveOpened)
	{
		OnReceiveOpened(Settings.ReceivePort);
	}

	UDPReceiver->Start();
}

void FUDPNative::CloseReceiveSocket()
{
	if (ReceiverSocket)
	{
		UDPReceiver->Stop();
		delete UDPReceiver;
		UDPReceiver = nullptr;

		ReceiverSocket->Close();
		ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(ReceiverSocket);
		ReceiverSocket = nullptr;

		if (OnReceiveClosed)
		{
			OnReceiveClosed(Settings.ReceivePort);
		}
	}

	Settings.bIsReceiveOpen = false;
}

void FUDPNative::ClearSendCallbacks()
{

	OnSendOpened = nullptr;
	OnSendClosed = nullptr;
}

void FUDPNative::ClearReceiveCallbacks()
{
	OnReceivedBytes = nullptr;
	OnReceiveOpened = nullptr;
	OnReceiveClosed = nullptr;
}

FUDPSettings::FUDPSettings()
{
	bShouldAutoOpenSend = true;
	bShouldAutoOpenReceive = true;
	bReceiveDataOnGameThread = true;
	SendIP = FString(TEXT("127.0.0.1"));
	SendPort = 3001;
	ReceivePort = 3002;
	SendSocketName = FString(TEXT("ue4-dgram-send"));
	ReceiveSocketName = FString(TEXT("ue4-dgram-receive"));

	BufferSize = 2 * 1024 * 1024;	//default roughly 2mb
}
