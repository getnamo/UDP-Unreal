
#include "UDPComponent.h"
#include "Networking.h"
#include "LambdaRunnable.h"
#include "UDPComponent.h"

UUDPComponent::UUDPComponent(const FObjectInitializer &init) : UActorComponent(init)
{
	bShouldAutoConnect = true;
	bWantsInitializeComponent = true;
	bAutoActivate = true;
	AddressAndPort = FString(TEXT("http://localhost:3000"));	//default to 127.0.0.1

	ReconnectionTimeout = 0.f;
	MaxReconnectionAttempts = -1.f;
	ReconnectionDelayInMs = 5000;
}

void UUDPComponent::Connect(const FString& InAddressAndPort)
{

}

void UUDPComponent::Disconnect()
{

}

void UUDPComponent::Emit(const TArray<uint8>& Bytes)
{

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
		Connect(AddressAndPort);
	}
}
