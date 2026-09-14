; w-seed-process-parallel-link0-windows-1
; Bounded CRT-free Windows x64 adapter for the W-1598 process/parallel
; composition. The four-slot frame is a private provider ABI for this witness,
; not a W Task representation or a public runtime ABI.

target triple = "x86_64-pc-windows-msvc"

%w_seed_process_parallel_frame = type { ptr, i64, i64, i32 }

@w_seed_parallel_force_failure = internal global i1 false, align 1

declare dllimport ptr @GetCommandLineW()
declare dllimport ptr @CreateThread(ptr, i64, ptr, ptr, i32, ptr)
declare dllimport i32 @WaitForSingleObject(ptr, i32)
declare dllimport i32 @CloseHandle(ptr)
declare dllimport void @ExitProcess(i32)

declare i64 @w_seed_parallel_task_0(ptr, ptr, i64)
declare i32 @w_seed_process_parallel_entry(ptr, ptr)

; Return 0 for no payload argument, 1 for an ordinary payload, and 2 for the
; gate-only provider-failure sentinel whose first payload character is '!'.
; This bounded scanner is deliberately not the general Windows Arguments
; provider: the public process route retains the complete provider contract.
define internal i32 @w_seed_argument_mode(ptr %command_line) {
entry:
  %first = load i16, ptr %command_line, align 2
  %quoted = icmp eq i16 %first, 34
  br i1 %quoted, label %quoted.loop, label %plain.loop

quoted.loop:
  %quoted.index = phi i64 [ 1, %entry ], [ %quoted.next, %quoted.more ]
  %quoted.address = getelementptr inbounds i16, ptr %command_line, i64 %quoted.index
  %quoted.character = load i16, ptr %quoted.address, align 2
  %quoted.zero = icmp eq i16 %quoted.character, 0
  %quoted.close = icmp eq i16 %quoted.character, 34
  %quoted.done = or i1 %quoted.zero, %quoted.close
  br i1 %quoted.done, label %quoted.end, label %quoted.more

quoted.more:
  %quoted.next = add i64 %quoted.index, 1
  br label %quoted.loop

quoted.end:
  %quoted.after = add i64 %quoted.index, 1
  br i1 %quoted.zero, label %empty, label %skip.loop

plain.loop:
  %plain.index = phi i64 [ 0, %entry ], [ %plain.next, %plain.more ]
  %plain.address = getelementptr inbounds i16, ptr %command_line, i64 %plain.index
  %plain.character = load i16, ptr %plain.address, align 2
  %plain.zero = icmp eq i16 %plain.character, 0
  %plain.space = icmp ule i16 %plain.character, 32
  %plain.done = or i1 %plain.zero, %plain.space
  br i1 %plain.done, label %plain.end, label %plain.more

plain.more:
  %plain.next = add i64 %plain.index, 1
  br label %plain.loop

plain.end:
  br i1 %plain.zero, label %empty, label %skip.loop

skip.loop:
  %skip.index = phi i64 [ %quoted.after, %quoted.end ],
                        [ %plain.index, %plain.end ],
                        [ %skip.next, %skip.more ]
  %skip.address = getelementptr inbounds i16, ptr %command_line, i64 %skip.index
  %skip.character = load i16, ptr %skip.address, align 2
  %skip.zero = icmp eq i16 %skip.character, 0
  br i1 %skip.zero, label %empty, label %skip.nonzero

skip.nonzero:
  %skip.space = icmp ule i16 %skip.character, 32
  br i1 %skip.space, label %skip.more, label %payload

skip.more:
  %skip.next = add i64 %skip.index, 1
  br label %skip.loop

payload:
  %failure = icmp eq i16 %skip.character, 33
  %mode = select i1 %failure, i32 2, i32 1
  ret i32 %mode

empty:
  ret i32 0
}

define i1 @w_seed_process_arguments_is_empty(ptr %arguments) {
entry:
  %value = load i8, ptr %arguments, align 1
  %empty = icmp ne i8 %value, 0
  ret i1 %empty
}

define internal i32 @w_seed_process_parallel_worker(ptr %raw) {
entry:
  %argument.address = getelementptr inbounds %w_seed_process_parallel_frame, ptr %raw, i32 0, i32 1
  %result.address = getelementptr inbounds %w_seed_process_parallel_frame, ptr %raw, i32 0, i32 2
  %status.address = getelementptr inbounds %w_seed_process_parallel_frame, ptr %raw, i32 0, i32 3
  %argument = load i64, ptr %argument.address, align 8
  %value = call i64 @w_seed_parallel_task_0(ptr null, ptr null, i64 %argument)
  store i64 %value, ptr %result.address, align 8
  store i32 0, ptr %status.address, align 4
  ret i32 0
}

define i1 @w_seed_parallel_launch_task_0(ptr %frame, i64 %argument) {
entry:
  %forced = load i1, ptr @w_seed_parallel_force_failure, align 1
  br i1 %forced, label %failure, label %launch

launch:
  %handle.address = getelementptr inbounds %w_seed_process_parallel_frame, ptr %frame, i32 0, i32 0
  %argument.address = getelementptr inbounds %w_seed_process_parallel_frame, ptr %frame, i32 0, i32 1
  %result.address = getelementptr inbounds %w_seed_process_parallel_frame, ptr %frame, i32 0, i32 2
  %status.address = getelementptr inbounds %w_seed_process_parallel_frame, ptr %frame, i32 0, i32 3
  store ptr null, ptr %handle.address, align 8
  store i64 %argument, ptr %argument.address, align 8
  store i64 0, ptr %result.address, align 8
  store i32 1, ptr %status.address, align 4
  %handle = call ptr @CreateThread(ptr null, i64 0, ptr @w_seed_process_parallel_worker, ptr %frame, i32 0, ptr null)
  store ptr %handle, ptr %handle.address, align 8
  %ok = icmp ne ptr %handle, null
  ret i1 %ok

failure:
  ret i1 false
}

define i1 @w_seed_parallel_join_task_0(ptr %frame, ptr %output) {
entry:
  %handle.address = getelementptr inbounds %w_seed_process_parallel_frame, ptr %frame, i32 0, i32 0
  %handle = load ptr, ptr %handle.address, align 8
  %present = icmp ne ptr %handle, null
  br i1 %present, label %wait, label %failure

wait:
  %wait.result = call i32 @WaitForSingleObject(ptr %handle, i32 -1)
  %close.result = call i32 @CloseHandle(ptr %handle)
  %wait.ok = icmp eq i32 %wait.result, 0
  %close.ok = icmp ne i32 %close.result, 0
  %status.address = getelementptr inbounds %w_seed_process_parallel_frame, ptr %frame, i32 0, i32 3
  %status = load i32, ptr %status.address, align 4
  %status.ok = icmp eq i32 %status, 0
  %argument.address = getelementptr inbounds %w_seed_process_parallel_frame, ptr %frame, i32 0, i32 1
  %stored.address = getelementptr inbounds %w_seed_process_parallel_frame, ptr %frame, i32 0, i32 2
  %argument = load i64, ptr %argument.address, align 8
  %stored = load i64, ptr %stored.address, align 8
  %expected = add i64 %argument, 1
  %semantic.ok = icmp eq i64 %stored, %expected
  store i64 %stored, ptr %output, align 8
  %wait.close.ok = and i1 %wait.ok, %close.ok
  %provider.ok = and i1 %wait.close.ok, %status.ok
  %all.ok = and i1 %provider.ok, %semantic.ok
  ret i1 %all.ok

failure:
  ret i1 false
}

define void @mainCRTStartup() {
entry:
  %arguments = alloca i8, align 1
  %command.line = call ptr @GetCommandLineW()
  %mode = call i32 @w_seed_argument_mode(ptr %command.line)
  %is.empty = icmp eq i32 %mode, 0
  %force.failure = icmp eq i32 %mode, 2
  store i1 %force.failure, ptr @w_seed_parallel_force_failure, align 1
  %is.empty.byte = zext i1 %is.empty to i8
  store i8 %is.empty.byte, ptr %arguments, align 1
  %status = call i32 @w_seed_process_parallel_entry(ptr %arguments, ptr null)
  call void @ExitProcess(i32 %status)
  unreachable
}
