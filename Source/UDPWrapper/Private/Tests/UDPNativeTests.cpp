#include "Misc/AutomationTest.h"
#include "Async/TaskGraphInterfaces.h"
#include "Containers/Ticker.h"
#include "UDPComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace UDPWrapperTests
{
	constexpr EAutomationTestFlags TestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;

	//Runs queued game thread tasks and one core ticker tick, roughly what a frame does for our deliveries
	void PumpGameThreadFrame()
	{
		FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
		FTSTicker::GetCoreTicker().Tick(0.f);
	}

	//Polls Condition until it's true or timeout, optionally pumping game thread tasks so AsyncTask callbacks run
	bool WaitUntil(TFunctionRef<bool()> Condition, double TimeoutSeconds, bool bPumpGameThread)
	{
		const double EndTime = FPlatformTime::Seconds() + TimeoutSeconds;
		while (FPlatformTime::Seconds() < EndTime)
		{
			if (bPumpGameThread)
			{
				PumpGameThreadFrame();
			}
			if (Condition())
			{
				return true;
			}
			FPlatformProcess::Sleep(0.01f);
		}
		return false;
	}

	TArray<uint8> MakePayload(const FString& Message)
	{
		FTCHARToUTF8 Utf8(*Message);
		return TArray<uint8>((const uint8*)Utf8.Get(), Utf8.Length());
	}

	struct FReceivedData
	{
		FCriticalSection Lock;
		TArray<TArray<uint8>> Packets;
		FString LastSenderIp;
		int32 LastSenderPort = 0;

		int32 Num()
		{
			FScopeLock ScopeLock(&Lock);
			return Packets.Num();
		}
	};

	void BindReceiver(FUDPNative& Receiver, TSharedRef<FReceivedData> Received)
	{
		Receiver.OnReceivedBytes = [Received](const TArray<uint8>& Data, const FString& Ip, const int32& Port)
		{
			FScopeLock ScopeLock(&Received->Lock);
			Received->Packets.Add(Data);
			Received->LastSenderIp = Ip;
			Received->LastSenderPort = Port;
		};
	}
}

using namespace UDPWrapperTests;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUDPNativeReceiveBackgroundThreadTest, "UDPWrapper.Native.ReceiveOnBackgroundThread", TestFlags)
bool FUDPNativeReceiveBackgroundThreadTest::RunTest(const FString& Parameters)
{
	const int32 Port = 38201;
	TSharedRef<FReceivedData> Received = MakeShared<FReceivedData>();

	FUDPNative Receiver;
	Receiver.Settings.bReceiveDataOnGameThread = false;
	BindReceiver(Receiver, Received);
	TestTrue(TEXT("Receive socket opened"), Receiver.OpenReceiveSocket(TEXT("0.0.0.0"), Port));

	FUDPNative Sender;
	const int32 BoundPort = Sender.OpenSendSocket(TEXT("127.0.0.1"), Port);
	TestTrue(TEXT("Send socket bound to a port"), BoundPort > 0);
	TestTrue(TEXT("Emit succeeded"), Sender.EmitBytes(MakePayload(TEXT("hello"))));

	TestTrue(TEXT("Packet received"), WaitUntil([&] { return Received->Num() > 0; }, 2.0, false));
	if (Received->Num() > 0)
	{
		TestEqual(TEXT("Payload matches"), Received->Packets[0], MakePayload(TEXT("hello")));
		TestEqual(TEXT("Sender port matches bound send port"), Received->LastSenderPort, BoundPort);
	}

	Sender.CloseSendSocket();
	Receiver.CloseReceiveSocket();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUDPNativeReceiveGameThreadTest, "UDPWrapper.Native.ReceiveOnGameThread", TestFlags)
bool FUDPNativeReceiveGameThreadTest::RunTest(const FString& Parameters)
{
	const int32 Port = 38202;
	TSharedRef<FReceivedData> Received = MakeShared<FReceivedData>();
	bool bReceivedOnGameThread = true;

	//Default settings, same as a UDP component with default properties
	FUDPNative Receiver;
	TestTrue(TEXT("Defaults receive on game thread"), Receiver.Settings.bReceiveDataOnGameThread);
	Receiver.OnReceivedBytes = [Received, &bReceivedOnGameThread](const TArray<uint8>& Data, const FString& Ip, const int32& Port)
	{
		bReceivedOnGameThread = bReceivedOnGameThread && IsInGameThread();
		FScopeLock ScopeLock(&Received->Lock);
		Received->Packets.Add(Data);
	};
	TestTrue(TEXT("Receive socket opened"), Receiver.OpenReceiveSocket(Receiver.Settings.ReceiveIP, Port));

	FUDPNative Sender;
	Sender.OpenSendSocket(TEXT("127.0.0.1"), Port);
	for (int32 i = 0; i < 10; i++)
	{
		Sender.EmitBytes(MakePayload(FString::Printf(TEXT("packet %d"), i)));
	}

	TestTrue(TEXT("All packets received"), WaitUntil([&] { return Received->Num() >= 10; }, 2.0, true));
	TestTrue(TEXT("Callbacks ran on game thread"), bReceivedOnGameThread);

	Sender.CloseSendSocket();
	Receiver.CloseReceiveSocket();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUDPNativeGameThreadBudgetTest, "UDPWrapper.Native.GameThreadTimeBudget", TestFlags)
bool FUDPNativeGameThreadBudgetTest::RunTest(const FString& Parameters)
{
	//Issue #26: a burst of packets with slow processing shouldn't all be delivered in one frame when a budget is set
	const int32 Port = 38207;
	const int32 PacketCount = 20;
	TSharedRef<FReceivedData> Received = MakeShared<FReceivedData>();

	FUDPNative Receiver;
	Receiver.Settings.ReceiveGameThreadTimeBudgetMs = 1.f;
	Receiver.OnReceivedBytes = [Received](const TArray<uint8>& Data, const FString& Ip, const int32& Port)
	{
		//Simulate expensive processing
		FPlatformProcess::Sleep(0.002f);
		FScopeLock ScopeLock(&Received->Lock);
		Received->Packets.Add(Data);
	};
	TestTrue(TEXT("Receive socket opened"), Receiver.OpenReceiveSocket(TEXT("0.0.0.0"), Port));

	FUDPNative Sender;
	Sender.OpenSendSocket(TEXT("127.0.0.1"), Port);
	for (int32 i = 0; i < PacketCount; i++)
	{
		Sender.EmitBytes(MakePayload(FString::Printf(TEXT("packet %d"), i)));
	}

	//Let the whole burst arrive before the game thread runs
	FPlatformProcess::Sleep(0.3f);

	PumpGameThreadFrame();
	const int32 DeliveredFirstFrame = Received->Num();
	TestTrue(FString::Printf(TEXT("First frame delivered some but not all packets (%d/%d)"), DeliveredFirstFrame, PacketCount), DeliveredFirstFrame > 0 && DeliveredFirstFrame < PacketCount);

	PumpGameThreadFrame();
	const int32 DeliveredSecondFrame = Received->Num() - DeliveredFirstFrame;
	TestTrue(FString::Printf(TEXT("Second frame delivered another slice (%d)"), DeliveredSecondFrame), DeliveredSecondFrame > 0 && Received->Num() < PacketCount);

	TestTrue(TEXT("Remaining packets delivered on later frames"), WaitUntil([&] { return Received->Num() >= PacketCount; }, 2.0, true));

	bool bInOrder = Received->Num() == PacketCount;
	for (int32 i = 0; bInOrder && i < PacketCount; i++)
	{
		bInOrder = Received->Packets[i] == MakePayload(FString::Printf(TEXT("packet %d"), i));
	}
	TestTrue(TEXT("Packets delivered in order"), bInOrder);

	Sender.CloseSendSocket();
	Receiver.CloseReceiveSocket();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUDPNativeDestroyedBeforeDeliveryTest, "UDPWrapper.Native.DestroyedBeforeGameThreadDelivery", TestFlags)
bool FUDPNativeDestroyedBeforeDeliveryTest::RunTest(const FString& Parameters)
{
	//Issue #43: game thread tasks queued by the receive thread must not touch a destroyed native.
	//NB: the pre-fix use-after-free reads freed memory and is only reliably caught with ASan, this checks no callbacks fire.
	const int32 Port = 38203;
	TSharedRef<FReceivedData> Received = MakeShared<FReceivedData>();

	TSharedPtr<FUDPNative> Receiver = MakeShared<FUDPNative>();
	BindReceiver(*Receiver, Received);
	TestTrue(TEXT("Receive socket opened"), Receiver->OpenReceiveSocket(TEXT("0.0.0.0"), Port));

	FUDPNative Sender;
	Sender.OpenSendSocket(TEXT("127.0.0.1"), Port);
	for (int32 i = 0; i < 20; i++)
	{
		Sender.EmitBytes(MakePayload(TEXT("in flight")));
	}

	//Let the receive thread queue game thread tasks, but don't run them yet
	FPlatformProcess::Sleep(0.3f);
	Receiver.Reset();

	PumpGameThreadFrame();
	TestEqual(TEXT("No callbacks after destruction"), Received->Num(), 0);

	Sender.CloseSendSocket();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUDPNativeBindFailureTest, "UDPWrapper.Native.BindFailure", TestFlags)
bool FUDPNativeBindFailureTest::RunTest(const FString& Parameters)
{
	//Binding to an address not on this machine (TEST-NET-1) should fail cleanly instead of crashing
	AddExpectedError(TEXT("failed to bind receive socket"), EAutomationExpectedErrorFlags::Contains, 1);

	FUDPNative Receiver;
	TestFalse(TEXT("Open receive on unavailable address fails"), Receiver.OpenReceiveSocket(TEXT("192.0.2.1"), 38204));
	TestFalse(TEXT("Receive not marked open"), Receiver.Settings.bIsReceiveOpen);
	TestTrue(TEXT("Close after failed open is safe"), Receiver.CloseReceiveSocket());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUDPNativeMulticastTest, "UDPWrapper.Native.Multicast", TestFlags)
bool FUDPNativeMulticastTest::RunTest(const FString& Parameters)
{
	//Issue #30: join a multicast group and receive data sent to it
	const int32 Port = 38205;
	const FString Group = TEXT("239.255.42.99");
	TSharedRef<FReceivedData> Received = MakeShared<FReceivedData>();

	FUDPNative Receiver;
	Receiver.Settings.bReceiveDataOnGameThread = false;
	Receiver.Settings.ReceiveMulticastGroupIP = Group;
	BindReceiver(Receiver, Received);
	TestTrue(TEXT("Receive socket joined group"), Receiver.OpenReceiveSocket(TEXT("0.0.0.0"), Port));

	FUDPNative Sender;
	TestTrue(TEXT("Send socket opened to group"), Sender.OpenSendSocket(Group, Port) > 0);
	for (int32 i = 0; i < 5; i++)
	{
		Sender.EmitBytes(MakePayload(TEXT("multicast")));
	}

	TestTrue(TEXT("Multicast packet received"), WaitUntil([&] { return Received->Num() > 0; }, 2.0, false));

	Sender.CloseSendSocket();
	Receiver.CloseReceiveSocket();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUDPNativeInvalidMulticastTest, "UDPWrapper.Native.InvalidMulticastGroup", TestFlags)
bool FUDPNativeInvalidMulticastTest::RunTest(const FString& Parameters)
{
	AddExpectedError(TEXT("is not a valid multicast address"), EAutomationExpectedErrorFlags::Contains, 1);

	FUDPNative Receiver;
	Receiver.Settings.ReceiveMulticastGroupIP = TEXT("10.0.0.1");
	TestFalse(TEXT("Non-multicast group is rejected"), Receiver.OpenReceiveSocket(TEXT("0.0.0.0"), 38206));
	return true;
}

#endif //WITH_DEV_AUTOMATION_TESTS
