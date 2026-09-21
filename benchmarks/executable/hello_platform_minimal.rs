#![no_std]
#![no_main]

// Expected exit: 0
// Expected stdout:
// Hello, world!

use core::panic::PanicInfo;

const GREETING: &[u8] = b"Hello, world!\n";

#[panic_handler]
fn panic(_: &PanicInfo<'_>) -> ! {
    platform_exit(1)
}

#[cfg(target_os = "windows")]
fn platform_exit(exit_code: u32) -> ! {
    #[link(name = "kernel32")]
    unsafe extern "system" {
        fn ExitProcess(exit_code: u32) -> !;
    }

    unsafe { ExitProcess(exit_code) }
}

#[cfg(target_os = "windows")]
#[unsafe(no_mangle)]
pub extern "system" fn w_entry() -> ! {
    use core::ffi::c_void;

    #[link(name = "kernel32")]
    unsafe extern "system" {
        fn GetStdHandle(standard_handle: u32) -> *mut c_void;
        fn WriteFile(
            file: *mut c_void,
            buffer: *const c_void,
            bytes_to_write: u32,
            bytes_written: *mut u32,
            overlapped: *mut c_void,
        ) -> i32;
    }

    let mut bytes_written = 0u32;
    let output = unsafe { GetStdHandle((-11i32) as u32) };
    let succeeded = unsafe {
        WriteFile(
            output,
            GREETING.as_ptr().cast(),
            GREETING.len() as u32,
            &mut bytes_written,
            core::ptr::null_mut(),
        )
    } != 0;
    let exit_code = if succeeded && bytes_written as usize == GREETING.len() {
        0
    } else {
        1
    };
    platform_exit(exit_code)
}

#[cfg(target_os = "linux")]
fn platform_exit(exit_code: u32) -> ! {
    use core::arch::asm;

    unsafe {
        asm!(
            "syscall",
            in("rax") 60usize,
            in("rdi") exit_code as usize,
            options(noreturn, nostack),
        )
    }
}

#[cfg(target_os = "linux")]
#[unsafe(no_mangle)]
pub extern "C" fn _start() -> ! {
    use core::arch::asm;

    let mut bytes_written = 0usize;
    unsafe {
        asm!(
            "syscall",
            inlateout("rax") 1usize => bytes_written,
            in("rdi") 1usize,
            in("rsi") GREETING.as_ptr(),
            in("rdx") GREETING.len(),
            lateout("rcx") _,
            lateout("r11") _,
            options(nostack),
        )
    }
    platform_exit(if bytes_written == GREETING.len() { 0 } else { 1 })
}
