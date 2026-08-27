// Rust independent baseline for BMD3 byte-scan-view.
// The program prints no output until the bounded input is fully validated.

use std::env;
use std::fs::File;
use std::io::{self, Read};

const MAX_INPUT_BYTES: u64 = 64 * 1024 * 1024;

fn parse_delimiter(text: &str) -> Option<u8> {
    if text.is_empty() || matches!(text.as_bytes().first(), Some(b'+' | b'-')) ||
        (text.as_bytes().first() == Some(&b'0') && text.len() != 1) {
        return None;
    }
    let value: u16 = text.parse().ok()?;
    u8::try_from(value).ok()
}

fn run(path: &str, delimiter: u8) -> io::Result<(u64, u64)> {
    let mut input = File::open(path)?;
    let mut buffer = [0_u8; 64 * 1024];
    let mut bytes = 0_u64;
    let mut matches = 0_u64;

    loop {
        let count = input.read(&mut buffer)?;
        if count == 0 {
            break;
        }
        let count = u64::try_from(count).expect("buffer length fits in u64");
        if bytes > MAX_INPUT_BYTES - count {
            return Err(io::Error::new(io::ErrorKind::InvalidData, "input exceeds 64 MiB"));
        }
        bytes += count;
        matches += buffer[..usize::try_from(count).expect("buffer length fits in usize")]
            .iter()
            .filter(|&&byte| byte == delimiter)
            .count() as u64;
    }
    Ok((bytes, matches))
}

fn main() {
    let arguments: Vec<String> = env::args().collect();
    if arguments.len() != 3 {
        std::process::exit(2);
    }
    let delimiter = match parse_delimiter(&arguments[2]) {
        Some(value) => value,
        None => std::process::exit(2),
    };
    let (bytes, matches) = match run(&arguments[1], delimiter) {
        Ok(value) => value,
        Err(_) => std::process::exit(3),
    };
    print!("{{\"bytes\":\"{}\",\"matches\":\"{}\"}}", bytes, matches);
}
