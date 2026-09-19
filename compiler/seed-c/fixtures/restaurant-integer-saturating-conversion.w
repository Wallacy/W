// Expected exit: 0
// Expected stdout:
// ss -128/7/127; us 7/127/127; su 0/200/255; uu 7/255/255; UInt->Int 9223372036854775807

fn signedToSigned(value: i16): i8 { return i8(saturating: value) }
fn unsignedToSigned(value: u16): i8 { return i8(saturating: value) }
fn signedToUnsigned(value: i16): u8 { return u8(saturating: value) }
fn unsignedToUnsigned(value: u16): u8 { return u8(saturating: value) }
fn aliasToAlias(value: UInt): Int { return Int(saturating: value) }

entry {
  let ssLow = signedToSigned(value: -129_i16)
  let ssInRange = signedToSigned(value: 7_i16)
  let ssHigh = signedToSigned(value: 128_i16)
  let usLow = unsignedToSigned(value: 7_u16)
  let usInRange = unsignedToSigned(value: 127_u16)
  let usHigh = unsignedToSigned(value: 128_u16)
  let suLow = signedToUnsigned(value: -1_i16)
  let suInRange = signedToUnsigned(value: 200_i16)
  let suHigh = signedToUnsigned(value: 256_i16)
  let uuLow = unsignedToUnsigned(value: 7_u16)
  let uuInRange = unsignedToUnsigned(value: 255_u16)
  let uuHigh = unsignedToUnsigned(value: 256_u16)
  let aliasHigh = aliasToAlias(value: 18446744073709551615_u64)
  print("ss ${ssLow}/${ssInRange}/${ssHigh}; us ${usLow}/${usInRange}/${usHigh}; su ${suLow}/${suInRange}/${suHigh}; uu ${uuLow}/${uuInRange}/${uuHigh}; UInt->Int ${aliasHigh}")
}
