; w-seed-parallel-link0-windows-1
; Internal PARLINK0 Windows adapter. It calls only compiler-generated task
; symbols, obtains a runtime-dependent value from Kernel32, and has no CRT.

target triple = "x86_64-pc-windows-msvc"

%w_parallel_job = type { i32, i64, i64, i64 }

declare dllimport i32 @GetCurrentProcessId()
declare dllimport ptr @CreateThread(ptr, i64, ptr, ptr, i32, ptr)
declare dllimport i32 @WaitForMultipleObjects(i32, ptr, i32, i32)
declare dllimport i32 @WaitForSingleObject(ptr, i32)
declare dllimport i32 @CloseHandle(ptr)
declare dllimport void @ExitProcess(i32)

declare i64 @w_seed_parallel_task_0(i64)
declare i64 @w_seed_parallel_task_1(i64, i64)

define internal i32 @w_parallel_worker(ptr %raw) {
entry:
  %ordinal.ptr = getelementptr inbounds %w_parallel_job, ptr %raw, i32 0, i32 0
  %ordinal = load i32, ptr %ordinal.ptr, align 8
  switch i32 %ordinal, label %invalid [
    i32 0, label %task0
    i32 1, label %task1
  ]

task0:
  %first0.ptr = getelementptr inbounds %w_parallel_job, ptr %raw, i32 0, i32 1
  %first0 = load i64, ptr %first0.ptr, align 8
  %value0 = call i64 @w_seed_parallel_task_0(i64 %first0)
  %result0.ptr = getelementptr inbounds %w_parallel_job, ptr %raw, i32 0, i32 3
  store i64 %value0, ptr %result0.ptr, align 8
  ret i32 0

task1:
  %first1.ptr = getelementptr inbounds %w_parallel_job, ptr %raw, i32 0, i32 1
  %second1.ptr = getelementptr inbounds %w_parallel_job, ptr %raw, i32 0, i32 2
  %first1 = load i64, ptr %first1.ptr, align 8
  %second1 = load i64, ptr %second1.ptr, align 8
  %value1 = call i64 @w_seed_parallel_task_1(i64 %first1, i64 %second1)
  %result1.ptr = getelementptr inbounds %w_parallel_job, ptr %raw, i32 0, i32 3
  store i64 %value1, ptr %result1.ptr, align 8
  ret i32 0

invalid:
  ret i32 1
}

define void @mainCRTStartup() {
entry:
  %jobs = alloca [2 x %w_parallel_job], align 8
  %handles = alloca [2 x ptr], align 8
  %job0 = getelementptr inbounds [2 x %w_parallel_job], ptr %jobs, i32 0, i32 0
  %job1 = getelementptr inbounds [2 x %w_parallel_job], ptr %jobs, i32 0, i32 1
  %pid32 = call i32 @GetCurrentProcessId()
  %pid = zext i32 %pid32 to i64

  %job0.ordinal = getelementptr inbounds %w_parallel_job, ptr %job0, i32 0, i32 0
  %job0.first = getelementptr inbounds %w_parallel_job, ptr %job0, i32 0, i32 1
  %job0.second = getelementptr inbounds %w_parallel_job, ptr %job0, i32 0, i32 2
  %job0.result = getelementptr inbounds %w_parallel_job, ptr %job0, i32 0, i32 3
  store i32 0, ptr %job0.ordinal, align 8
  store i64 %pid, ptr %job0.first, align 8
  store i64 0, ptr %job0.second, align 8
  store i64 0, ptr %job0.result, align 8

  %job1.ordinal = getelementptr inbounds %w_parallel_job, ptr %job1, i32 0, i32 0
  %job1.first = getelementptr inbounds %w_parallel_job, ptr %job1, i32 0, i32 1
  %job1.second = getelementptr inbounds %w_parallel_job, ptr %job1, i32 0, i32 2
  %job1.result = getelementptr inbounds %w_parallel_job, ptr %job1, i32 0, i32 3
  store i32 1, ptr %job1.ordinal, align 8
  store i64 %pid, ptr %job1.first, align 8
  store i64 2, ptr %job1.second, align 8
  store i64 0, ptr %job1.result, align 8

  %thread0 = call ptr @CreateThread(ptr null, i64 0, ptr @w_parallel_worker,
                                    ptr %job0, i32 0, ptr null)
  %thread0.ok = icmp ne ptr %thread0, null
  br i1 %thread0.ok, label %create1, label %provider.failure

create1:
  %thread1 = call ptr @CreateThread(ptr null, i64 0, ptr @w_parallel_worker,
                                    ptr %job1, i32 0, ptr null)
  %thread1.ok = icmp ne ptr %thread1, null
  br i1 %thread1.ok, label %wait, label %cleanup0.failure

cleanup0.failure:
  %wait0.failure = call i32 @WaitForSingleObject(ptr %thread0, i32 -1)
  %close0.failure = call i32 @CloseHandle(ptr %thread0)
  br label %provider.failure

wait:
  %handle0 = getelementptr inbounds [2 x ptr], ptr %handles, i32 0, i32 0
  %handle1 = getelementptr inbounds [2 x ptr], ptr %handles, i32 0, i32 1
  store ptr %thread0, ptr %handle0, align 8
  store ptr %thread1, ptr %handle1, align 8
  %wait.result = call i32 @WaitForMultipleObjects(i32 2, ptr %handles,
                                                   i32 1, i32 -1)
  %wait.ok = icmp eq i32 %wait.result, 0
  br i1 %wait.ok, label %joined, label %join.failure

join.failure:
  %wait0.retry = call i32 @WaitForSingleObject(ptr %thread0, i32 -1)
  %wait1.retry = call i32 @WaitForSingleObject(ptr %thread1, i32 -1)
  br label %joined.failure

joined.failure:
  %close0.join.failure = call i32 @CloseHandle(ptr %thread0)
  %close1.join.failure = call i32 @CloseHandle(ptr %thread1)
  br label %provider.failure

joined:
  %close0 = call i32 @CloseHandle(ptr %thread0)
  %close1 = call i32 @CloseHandle(ptr %thread1)
  %result0 = load i64, ptr %job0.result, align 8
  %result1 = load i64, ptr %job1.result, align 8
  %expected0 = add i64 %pid, 1
  %expected1 = add i64 %pid, 3
  %result0.ok = icmp eq i64 %result0, %expected0
  %result1.ok = icmp eq i64 %result1, %expected1
  %results.ok = and i1 %result0.ok, %result1.ok
  br i1 %results.ok, label %success, label %semantic.failure

success:
  call void @ExitProcess(i32 0)
  unreachable

semantic.failure:
  call void @ExitProcess(i32 4)
  unreachable

provider.failure:
  call void @ExitProcess(i32 3)
  unreachable
}
