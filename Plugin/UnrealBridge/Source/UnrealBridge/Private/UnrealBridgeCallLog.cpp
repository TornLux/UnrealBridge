#include "UnrealBridgeCallLog.h"

FBridgeCallLog& FBridgeCallLog::Get()
{
	static FBridgeCallLog Instance;
	return Instance;
}

void FBridgeCallLog::Append(FBridgeCallRecord&& Record)
{
	FScopeLock L(&Lock);
	Entries.Add(MoveTemp(Record));
	while (Entries.Num() > Capacity)
	{
		Entries.RemoveAt(0, /*Count*/ 1, /*EAllowShrinking*/ EAllowShrinking::No);
		++TotalDropped;
	}
}

int32 FBridgeCallLog::Clear()
{
	FScopeLock L(&Lock);
	const int32 N = Entries.Num();
	Entries.Reset();
	return N;
}

int32 FBridgeCallLog::GetCapacity() const
{
	FScopeLock L(&Lock);
	return Capacity;
}

int32 FBridgeCallLog::SetCapacity(int32 NewCap)
{
	FScopeLock L(&Lock);
	Capacity = NewCap;
	while (Entries.Num() > Capacity)
	{
		Entries.RemoveAt(0, /*Count*/ 1, /*EAllowShrinking*/ EAllowShrinking::No);
		++TotalDropped;
	}
	return Capacity;
}

int32 FBridgeCallLog::GetTotalDropped() const
{
	FScopeLock L(&Lock);
	return TotalDropped;
}

TArray<FBridgeCallRecord> FBridgeCallLog::Snapshot(int32 MaxEntries) const
{
	FScopeLock L(&Lock);
	if (MaxEntries <= 0 || MaxEntries >= Entries.Num())
	{
		return Entries;
	}
	TArray<FBridgeCallRecord> Out;
	Out.Reserve(MaxEntries);
	const int32 Start = Entries.Num() - MaxEntries;
	for (int32 i = Start; i < Entries.Num(); ++i)
	{
		Out.Add(Entries[i]);
	}
	return Out;
}
