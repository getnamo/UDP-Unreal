
#include "UDPComponent.h"
#include "Async/Async.h"
#include "Containers/Ticker.h"
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
	//Callbacks may be invoked after this component is destroyed (e.g. level transition), guard with a weak ptr
	TWeakObjectPtr<UUDPComponent> WeakThis = this;

	Native->OnSendOpened = [WeakThis](int32 SpecifiedPort, int32 BoundPort, FString BoundIP)
	{
		if (!WeakThis.IsValid())
		{
			return;
		}
		FUDPSettings& Settings = WeakThis->Settings;
		Settings.bIsSendOpen = true;
		Settings.SendBoundPort = BoundPort;	//ensure sync on opened bound port
		Settings.SendBoundIP = BoundIP;

		Settings.SendIP = WeakThis->Native->Settings.SendIP;
		Settings.SendPort = WeakThis->Native->Settings.SendPort;

		WeakThis->OnSendSocketOpened.Broadcast(Settings.SendPort, Settings.SendBoundPort, Settings.SendBoundIP);
	};
	Native->OnSendClosed = [WeakThis](int32 Port)
	{
		if (!WeakThis.IsValid())
		{
			return;
		}
		WeakThis->Settings.bIsSendOpen = false;
		WeakThis->OnSendSocketClosed.Broadcast(Port);
	};
	Native->OnReceiveOpened = [WeakThis](int32 Port)
	{
		if (!WeakThis.IsValid())
		{
			return;
		}
		FUDPSettings& Settings = WeakThis->Settings;
		Settings.ReceiveIP = WeakThis->Native->Settings.ReceiveIP;
		Settings.ReceivePort = WeakThis->Native->Settings.ReceivePort;

		Settings.bIsReceiveOpen = true;
		WeakThis->OnReceiveSocketOpened.Broadcast(Port);
	};
	Native->OnReceiveClosed = [WeakThis](int32 Port)
	{
		if (!WeakThis.IsValid())
		{
			return;
		}
		WeakThis->Settings.bIsReceiveOpen = false;
		WeakThis->OnReceiveSocketClosed.Broadcast(Port);
	};
	Native->OnReceivedBytes = [WeakThis](const TArray<uint8>& Data, const FString& Endpoint, const int32& Port)
	{
		if (!WeakThis.IsValid())
		{
			return;
		}
		WeakThis->OnReceivedBytes.Broadcast(Data, Endpoint, Port);
	};
}

bool UUDPComponent::CloseReceiveSocket()
{
	return Native->CloseReceiveSocket();
}

int32 UUDPComponent::OpenSendSocket(const FString& InIP /*= TEXT("127.0.0.1")*/, const int32 InPort /*= 3000*/)
{
	//Sync side effect sampled settings
	Native->Settings.bShouldAutoOpenSend = Settings.bShouldAutoOpenSend;
	Native->Settings.SendSocketName = Settings.SendSocketName;
	Native->Settings.BufferSize = Settings.BufferSize;

	return Native->OpenSendSocket(InIP, InPort);
}

bool UUDPComponent::CloseSendSocket()
{
	Settings.SendBoundPort = 0;
	Settings.SendBoundIP = FString(TEXT("0.0.0.0"));
	return Native->CloseSendSocket();
}

bool UUDPComponent::OpenReceiveSocket(const FString& InListenIp /*= TEXT("0.0.0.0")*/, const int32 InListenPort /*= 3002*/)
{
	//Sync side effect sampled settings
	Native->Settings.bShouldAutoOpenReceive = Settings.bShouldAutoOpenReceive;
	Native->Settings.bShouldOpenReceiveToBoundSendPort = Settings.bShouldOpenReceiveToBoundSendPort;
	Native->Settings.bReceiveDataOnGameThread = Settings.bReceiveDataOnGameThread;
	Native->Settings.ReceiveGameThreadTimeBudgetMs = Settings.ReceiveGameThreadTimeBudgetMs;
	Native->Settings.ReceiveMulticastGroupIP = Settings.ReceiveMulticastGroupIP;
	Native->Settings.ReceiveSocketName = Settings.ReceiveSocketName;
	Native->Settings.BufferSize = Settings.BufferSize;

	return Native->OpenReceiveSocket(InListenIp, InListenPort);
}

bool UUDPComponent::EmitBytes(const TArray<uint8>& Bytes)
{
	return Native->EmitBytes(Bytes);
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
	
	//Sync all settings to native. These are duplicated for dev convenience in bp
	Native->Settings = Settings;

	if (Settings.bShouldAutoOpenSend)
	{
		OpenSendSocket(Settings.SendIP, Settings.SendPort);
	}

	if (Settings.bShouldAutoOpenReceive)
	{
		OpenReceiveSocket(Settings.ReceiveIP, Settings.ReceivePort);
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
	UDPReceiver = nullptr;
	SocketSubsystem = nullptr;

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

int32 FUDPNative::OpenSendSocket(const FString& InIP /*= TEXT("127.0.0.1")*/, const int32 InPort /*= 3000*/)
{
	Settings.SendIP = InIP;
	Settings.SendPort = InPort;

	RemoteAdress = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateInternetAddr();

	bool bIsValid;
	RemoteAdress->SetIp(*Settings.SendIP, bIsValid);
	RemoteAdress->SetPort(Settings.SendPort);

	if (!bIsValid)
	{
		UE_LOG(LogTemp, Error, TEXT("UDP address is invalid <%s:%d>"), *Settings.SendIP, Settings.SendPort);
		return 0;
	}

	//Don't leak a previously opened socket
	if (SenderSocket)
	{
		CloseSendSocket();
	}

	//Multicast loopback restores the OS default (the builder disables it) so local listeners receive multicast sends
	SenderSocket = FUdpSocketBuilder(*Settings.SendSocketName).AsReusable().WithBroadcast().WithMulticastLoopback();

	if (!SenderSocket)
	{
		UE_LOG(LogTemp, Error, TEXT("UDP failed to create send socket for <%s:%d>"), *Settings.SendIP, Settings.SendPort);
		return 0;
	}

	//Set Send Buffer Size
	SenderSocket->SetSendBufferSize(Settings.BufferSize, Settings.BufferSize);
	SenderSocket->SetReceiveBufferSize(Settings.BufferSize, Settings.BufferSize);

	bool bDidConnect = SenderSocket->Connect(*RemoteAdress);
	Settings.bIsSendOpen = true;
	Settings.SendBoundPort = SenderSocket->GetPortNo();
	TSharedRef<FInternetAddr> SendBoundAddress = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateInternetAddr();
	SenderSocket->GetAddress(*SendBoundAddress);
	Settings.SendBoundIP = SendBoundAddress->ToString(false);

	if (OnSendOpened)
	{	
		OnSendOpened(Settings.SendPort, Settings.SendBoundPort, Settings.SendBoundIP);
	}

	return Settings.SendBoundPort;
}

bool FUDPNative::CloseSendSocket()
{
	bool bDidCloseCorrectly = true;
	Settings.bIsSendOpen = false;

	if (SenderSocket)
	{
		bDidCloseCorrectly = SenderSocket->Close();
		ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(SenderSocket);
		SenderSocket = nullptr;

		if (OnSendClosed)
		{
			OnSendClosed(Settings.SendPort);
		}
	}

	return bDidCloseCorrectly;
}

bool FUDPNative::EmitBytes(const TArray<uint8>& Bytes)
{
	bool bDidSendCorrectly = true;

	if (SenderSocket && SenderSocket->GetConnectionState() == SCS_Connected)
	{
		int32 BytesSent = 0;
		bDidSendCorrectly = SenderSocket->Send(Bytes.GetData(), Bytes.Num(), BytesSent);
	}
	else if(Settings.bShouldAutoOpenSend)
	{
		bool bDidOpen = OpenSendSocket(Settings.SendIP, Settings.SendPort) != 0;
		return bDidOpen && EmitBytes(Bytes);
	}

	return bDidSendCorrectly;
}

bool FUDPNative::OpenReceiveSocket(const FString& InListenIP /*= TEXT("0.0.0.0")*/, const int32 InListenPort /*= 3002*/)
{
	//Sync and overwrite settings
	if (Settings.bShouldOpenReceiveToBoundSendPort)
	{
		if (Settings.SendBoundPort == 0)
		{
			UE_LOG(LogTemp, Error, TEXT("FUDPNative::OpenReceiveSocket Can't bind to SendBoundPort if send socket hasn't been opened before this call."));
			return false;
		}
		Settings.ReceiveIP = Settings.SendBoundIP;
		Settings.ReceivePort = Settings.SendBoundPort;
	}
	else
	{
		Settings.ReceiveIP = InListenIP;
		Settings.ReceivePort = InListenPort;
	}

	bool bDidOpenCorrectly = true;

	if (Settings.bIsReceiveOpen)
	{
		bDidOpenCorrectly = CloseReceiveSocket();
	}

	FIPv4Address Addr;
	FIPv4Address::Parse(Settings.ReceiveIP, Addr);

	//Create Socket
	FIPv4Endpoint Endpoint(Addr, Settings.ReceivePort);

	FUdpSocketBuilder Builder = FUdpSocketBuilder(*Settings.ReceiveSocketName)
		.AsNonBlocking()
		.AsReusable()
		.BoundToEndpoint(Endpoint)
		.WithReceiveBufferSize(Settings.BufferSize);

	if (!Settings.ReceiveMulticastGroupIP.IsEmpty())
	{
		FIPv4Address GroupAddr;
		if (!FIPv4Address::Parse(Settings.ReceiveMulticastGroupIP, GroupAddr) || !GroupAddr.IsMulticastAddress())
		{
			UE_LOG(LogTemp, Error, TEXT("UDP multicast group <%s> is not a valid multicast address"), *Settings.ReceiveMulticastGroupIP);
			return false;
		}
		//Loopback so packets sent to the group from this machine are also received (windows applies this on the receive side)
		Builder.JoinedToGroup(GroupAddr).WithMulticastLoopback();
	}

	ReceiverSocket = Builder.Build();

	if (!ReceiverSocket)
	{
		UE_LOG(LogTemp, Error, TEXT("UDP failed to bind receive socket to <%s:%d>"), *Settings.ReceiveIP, Settings.ReceivePort);
		return false;
	}

	FTimespan ThreadWaitTime = FTimespan::FromMilliseconds(100);
	FString ThreadName = FString::Printf(TEXT("UDP RECEIVER-FUDPNative"));
	UDPReceiver = new FUdpSocketReceiver(ReceiverSocket, ThreadWaitTime, *ThreadName);

	TWeakPtr<bool, ESPMode::ThreadSafe> WeakLifetime = LifetimeToken;

	UDPReceiver->OnDataReceived().BindLambda([this, WeakLifetime](const FArrayReaderPtr& DataPtr, const FIPv4Endpoint& Endpoint)
	{
		if (!OnReceivedBytes)
		{
			return;
		}

		TArray<uint8> Data;
		Data.AddUninitialized(DataPtr->TotalSize());
		DataPtr->Serialize(Data.GetData(), DataPtr->TotalSize());

		FString SenderIp = Endpoint.Address.ToString();
		int32 SenderPort = Endpoint.Port;

		if (Settings.bReceiveDataOnGameThread)
		{
			//Queue data for the game thread, one drain task handles a whole burst of packets
			ReceiveQueue.Enqueue({ MoveTemp(Data), MoveTemp(SenderIp), SenderPort });

			if (!bReceiveDrainScheduled.exchange(true))
			{
				AsyncTask(ENamedThreads::GameThread, [this, WeakLifetime]()
				{
					//this native may have been destroyed before the task ran
					if (WeakLifetime.IsValid())
					{
						DrainReceiveQueue();
					}
				});
			}
		}
		else
		{
			OnReceivedBytes(Data, SenderIp, SenderPort);
		}
	});

	Settings.bIsReceiveOpen = true;

	if (OnReceiveOpened)
	{
		OnReceiveOpened(Settings.ReceivePort);
	}

	UDPReceiver->Start();

	return bDidOpenCorrectly;
}

bool FUDPNative::CloseReceiveSocket()
{
	bool bDidCloseCorrectly = true;
	Settings.bIsReceiveOpen = false;

	if (ReceiverSocket)
	{
		UDPReceiver->Stop();
		delete UDPReceiver;
		UDPReceiver = nullptr;

		//Don't deliver packets after close
		ReceiveQueue.Empty();

		bDidCloseCorrectly = ReceiverSocket->Close();
		ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(ReceiverSocket);
		ReceiverSocket = nullptr;

		if (OnReceiveClosed)
		{
			OnReceiveClosed(Settings.ReceivePort);
		}
	}

	return bDidCloseCorrectly;
}

void FUDPNative::DrainReceiveQueue()
{
	//Packets queued from here on schedule another drain
	bReceiveDrainScheduled = false;

	if (DeliverQueuedPackets())
	{
		return;
	}

	//Over budget, deliver the rest on following frames so the game thread can't be starved by incoming data.
	//A single ticker drains one budget slice per frame (tickers added during a tick run in that same tick).
	bReceiveDrainScheduled = true;

	TWeakPtr<bool, ESPMode::ThreadSafe> WeakLifetime = LifetimeToken;
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([this, WeakLifetime](float DeltaTime)
	{
		if (!WeakLifetime.IsValid())
		{
			return false;
		}
		if (!DeliverQueuedPackets())
		{
			return true;
		}

		bReceiveDrainScheduled = false;

		//A packet may have been queued before the flag cleared, keep ticking to deliver it
		return !ReceiveQueue.IsEmpty() && !bReceiveDrainScheduled.exchange(true);
	}));
}

bool FUDPNative::DeliverQueuedPackets()
{
	const double BudgetSeconds = Settings.ReceiveGameThreadTimeBudgetMs / 1000.0;
	const double StartTime = FPlatformTime::Seconds();

	FReceivedPacket Packet;
	while (ReceiveQueue.Dequeue(Packet))
	{
		if (OnReceivedBytes)
		{
			OnReceivedBytes(Packet.Data, Packet.SenderIp, Packet.SenderPort);
		}

		if (BudgetSeconds > 0.0 && !ReceiveQueue.IsEmpty() && (FPlatformTime::Seconds() - StartTime) > BudgetSeconds)
		{
			return false;
		}
	}
	return true;
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
	bShouldOpenReceiveToBoundSendPort = false;
	bReceiveDataOnGameThread = true;
	ReceiveGameThreadTimeBudgetMs = 0.f;
	SendIP = FString(TEXT("127.0.0.1"));
	SendPort = 3001;
	SendBoundPort = 0;	//invalid if 0
	SendBoundIP = FString(TEXT("0.0.0.0"));
	ReceiveIP = FString(TEXT("0.0.0.0"));
	ReceivePort = 3002;
	SendSocketName = FString(TEXT("ue4-dgram-send"));
	ReceiveSocketName = FString(TEXT("ue4-dgram-receive"));

	bIsReceiveOpen = false;
	bIsSendOpen = false;

	BufferSize = 2 * 1024 * 1024;	//default roughly 2mb
}
