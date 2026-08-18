#pragma once

#include "CoreMinimal.h"
#include "HAL/Event.h"
#include "HAL/PlatformProcess.h"
#include "Misc/ScopeLock.h"

/**
 * 可取消 work item 的生命周期；只有 queued 可以进入 running 或 cancelled。
 * Lifecycle for cancellable work; only queued work may become running or cancelled.
 */
enum class EUnrealBridgeWorkState : uint8
{
	Queued,
	Running,
	Cancelled,
	Completed,
};

/**
 * 串行化 Server shutdown 与新 work 的注册；Close 返回后任何 callback 都不能再进入。
 * Serializes Server shutdown against new work registration; no callback can enter after Close returns.
 */
class FUnrealBridgeWorkAdmissionGate final
{
public:
	/** 开放新一轮 Server admission。 / Open admission for a new Server run. */
	void Open()
	{
		FScopeLock Lock(&GateLock);
		bOpen = true;
	}

	/** 关闭 admission，并等待已经进入的注册 callback 离开。 / Close admission and wait for entered callbacks to leave. */
	void Close()
	{
		FScopeLock Lock(&GateLock);
		bOpen = false;
	}

	/**
	 * 只在 gate 开放时、持锁执行注册 callback，使 enqueue/task registration 与 Close 原子排序。
	 * Run registration under the gate lock only while open, atomically ordering enqueue/task registration with Close.
	 */
	template <typename CallbackType>
	bool TryAdmit(CallbackType&& Callback)
	{
		FScopeLock Lock(&GateLock);
		if (!bOpen)
		{
			return false;
		}
		Forward<CallbackType>(Callback)();
		return true;
	}

	/** 返回当前 admission 状态。 / Return the current admission state. */
	bool IsOpen() const
	{
		FScopeLock Lock(&GateLock);
		return bOpen;
	}

private:
	mutable FCriticalSection GateLock;
	bool bOpen = false;
};

/**
 * 在 worker 与 GameThread 之间唯一拥有 work 状态、result 和 completion event。
 * Sole owner of work state, result, and completion event across worker/GameThread.
 *
 * 同一把锁使状态转换与 result 发布成为不可分割的观测边界；body 在锁外执行，
 * 因此 timeout 可以观察 running 并明确报告结果未知，而不会把已开始的动作误报为已取消。
 * One lock makes state transition and result publication a single observation boundary. The body
 * runs outside the lock so a timeout can observe running and report an unknown outcome without
 * falsely claiming that already-started work was cancelled.
 */
template <typename TResult>
class TUnrealBridgeCancellableWork final
{
public:
	explicit TUnrealBridgeCancellableWork(TFunction<TResult()>&& InBody)
		: Body(MoveTemp(InBody))
		, CompletionEvent(FPlatformProcess::GetSynchEventFromPool(true))
	{
		check(CompletionEvent != nullptr);
	}

	~TUnrealBridgeCancellableWork()
	{
		if (CompletionEvent != nullptr)
		{
			FPlatformProcess::ReturnSynchEventToPool(CompletionEvent);
			CompletionEvent = nullptr;
		}
	}

	TUnrealBridgeCancellableWork(const TUnrealBridgeCancellableWork&) = delete;
	TUnrealBridgeCancellableWork& operator=(const TUnrealBridgeCancellableWork&) = delete;

	/**
	 * 仅当状态仍为 queued 时执行 body；晚到的 consumer 会观察 cancelled 并跳过。
	 * Execute only while queued; a late consumer observes cancelled and skips the body.
	 */
	bool TryExecute()
	{
		{
			FScopeLock Lock(&StateLock);
			if (State != EUnrealBridgeWorkState::Queued)
			{
				return false;
			}
			State = EUnrealBridgeWorkState::Running;
		}

		TResult WorkResult = Body();

		{
			FScopeLock Lock(&StateLock);
			check(State == EUnrealBridgeWorkState::Running);
			Result.Emplace(MoveTemp(WorkResult));
			State = EUnrealBridgeWorkState::Completed;
		}
		CompletionEvent->Trigger();
		return true;
	}

	/**
	 * 仅取消尚未开始的 queued work，并以调用方提供的结果完成等待。
	 * Cancel only queued work and complete the wait with the caller-supplied result.
	 *
	 * @param CancellationResult 取消成功时发布的结果。 / Result published when cancellation wins.
	 * @param OutObservedState 返回取消尝试结束时的权威状态。 / Authoritative state observed by the attempt.
	 * @return true 表示 body 永远不会执行；false 表示已开始或已终止。 / True means the body can never execute.
	 */
	bool TryCancel(TResult&& CancellationResult, EUnrealBridgeWorkState& OutObservedState)
	{
		{
			FScopeLock Lock(&StateLock);
			if (State != EUnrealBridgeWorkState::Queued)
			{
				OutObservedState = State;
				return false;
			}

			Result.Emplace(MoveTemp(CancellationResult));
			State = EUnrealBridgeWorkState::Cancelled;
			OutObservedState = State;
		}
		CompletionEvent->Trigger();
		return true;
	}

	/** 等待 terminal result，返回 false 表示 deadline 前未完成。 / Wait for a terminal result; false means the deadline elapsed. */
	bool WaitFor(const FTimespan& Timeout) const
	{
		return CompletionEvent->Wait(Timeout);
	}

	/** 返回 terminal result 的副本；调用前必须已经完成或取消。 / Return a copy of the terminal result; wait first. */
	TResult GetResult() const
	{
		FScopeLock Lock(&StateLock);
		check(Result.IsSet());
		return Result.GetValue();
	}

	/** 返回当前权威状态。 / Return the current authoritative state. */
	EUnrealBridgeWorkState GetState() const
	{
		FScopeLock Lock(&StateLock);
		return State;
	}

private:
	TFunction<TResult()> Body;
	mutable FCriticalSection StateLock;
	TOptional<TResult> Result;
	EUnrealBridgeWorkState State = EUnrealBridgeWorkState::Queued;
	FEvent* CompletionEvent = nullptr;
};
