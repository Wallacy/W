#![no_std]

use core::arch::asm;
use core::ffi::c_void;
use core::panic::PanicInfo;

unsafe extern "C" {
    fn w_seed_process_entry0_context_drop(context: *mut c_void) -> i32;
    fn w_seed_process_entry0_arguments_drop(arguments: *mut c_void) -> i32;
}

#[panic_handler]
fn panic(_info: &PanicInfo<'_>) -> ! {
    trap()
}

fn trap() -> ! {
    // Keep the private baseline's fault boundary equivalent to LLVM's UD2.
    unsafe { asm!("ud2", options(noreturn)) }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn w_seed_process_entry0_handler(
    arguments: *mut c_void,
    context: *mut c_void,
) -> i32 {
    let context_status = unsafe { w_seed_process_entry0_context_drop(context) };
    if context_status != 0 {
        trap();
    }

    let arguments_status =
        unsafe { w_seed_process_entry0_arguments_drop(arguments) };
    if arguments_status != 0 {
        trap();
    }

    0
}
