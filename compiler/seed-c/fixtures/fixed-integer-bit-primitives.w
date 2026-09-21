// Expected exit: 0
// Expected stdout:
// i8 3/5/1/1 74/127 82 -92/41
// u8 4/4/0/1 105 150 45/75
// i16 5/11/3/2 11336/32767 13330 9320/2330
// u16 8/8/0/0 54673 43913 4951/50389
// i32 13/19/3/3 510274632/2147483647 2018915346 610839792/152709948
// u32 20/12/0/0 4155757969 4023233417 324508639/3302352631
// i64 30/34/7/1 8553414939923104896/9223372036854775807 7984226321029210881 163971058432973532/40992764608243383
// u64 32/32/0/4 597899502893742975 1167088121787636990 18282773015276577825/9182379272246532360

fn row0() {
 let a = i8.countOnes(0x52_i8)
 let b = i8.countZeros(0x52_i8)
 let c = i8.countLeadingZeros(0x52_i8)
 let d = i8.countTrailingZeros(0x52_i8)
 let e = i8.reversedBits(~1_i8)
 let g = i8.reversedBytes(0x52_i8)
 let h = i8.rotatedLeft(0x52_i8, 9_u64)
 let j = i8.rotatedRight(0x52_i8, 9_u64)
 print("i8 ${a}/${b}/${c}/${d} 74/${e} ${g} ${h}/${j}")
}
fn row1() {
 let a = u8.countOnes(0x96_u8)
 let b = u8.countZeros(0x96_u8)
 let c = u8.countLeadingZeros(0x96_u8)
 let d = u8.countTrailingZeros(0x96_u8)
 let e = u8.reversedBits(0x96_u8)
 let f = u8.reversedBytes(0x96_u8)
 let h = u8.rotatedLeft(0x96_u8, 9_u64)
 let i = u8.rotatedRight(0x96_u8, 9_u64)
 print("u8 ${a}/${b}/${c}/${d} ${e} ${f} ${h}/${i}")
}
fn row2() {
 let a = i16.countOnes(0x1234_i16)
 let b = i16.countZeros(0x1234_i16)
 let c = i16.countLeadingZeros(0x1234_i16)
 let d = i16.countTrailingZeros(0x1234_i16)
 let e = i16.reversedBits(~1_i16)
 let g = i16.reversedBytes(0x1234_i16)
 let h = i16.rotatedLeft(0x1234_i16, 17_u64)
 let j = i16.rotatedRight(0x1234_i16, 17_u64)
 print("i16 ${a}/${b}/${c}/${d} 11336/${e} ${g} ${h}/${j}")
}
fn row3() {
 let a = u16.countOnes(0x89ab_u16)
 let b = u16.countZeros(0x89ab_u16)
 let c = u16.countLeadingZeros(0x89ab_u16)
 let d = u16.countTrailingZeros(0x89ab_u16)
 let e = u16.reversedBits(0x89ab_u16)
 let f = u16.reversedBytes(0x89ab_u16)
 let h = u16.rotatedLeft(0x89ab_u16, 17_u64)
 let i = u16.rotatedRight(0x89ab_u16, 17_u64)
 print("u16 ${a}/${b}/${c}/${d} ${e} ${f} ${h}/${i}")
}
fn row4() {
 let a = i32.countOnes(0x12345678_i32)
 let b = i32.countZeros(0x12345678_i32)
 let c = i32.countLeadingZeros(0x12345678_i32)
 let d = i32.countTrailingZeros(0x12345678_i32)
 let e = i32.reversedBits(~1_i32)
 let g = i32.reversedBytes(0x12345678_i32)
 let h = i32.rotatedLeft(0x12345678_i32, 33_u64)
 let j = i32.rotatedRight(0x12345678_i32, 33_u64)
 print("i32 ${a}/${b}/${c}/${d} 510274632/${e} ${g} ${h}/${j}")
}
fn row5() {
 let a = u32.countOnes(0x89abcdef_u32)
 let b = u32.countZeros(0x89abcdef_u32)
 let c = u32.countLeadingZeros(0x89abcdef_u32)
 let d = u32.countTrailingZeros(0x89abcdef_u32)
 let e = u32.reversedBits(0x89abcdef_u32)
 let f = u32.reversedBytes(0x89abcdef_u32)
 let h = u32.rotatedLeft(0x89abcdef_u32, 33_u64)
 let i = u32.rotatedRight(0x89abcdef_u32, 33_u64)
 print("u32 ${a}/${b}/${c}/${d} ${e} ${f} ${h}/${i}")
}
fn row6() {
 let a = i64.countOnes(0x0123456789abcd6e_i64)
 let b = i64.countZeros(0x0123456789abcd6e_i64)
 let c = i64.countLeadingZeros(0x0123456789abcd6e_i64)
 let d = i64.countTrailingZeros(0x0123456789abcd6e_i64)
 let e = i64.reversedBits(~1_i64)
 let g = i64.reversedBytes(0x0123456789abcd6e_i64)
 let h = i64.rotatedLeft(0x0123456789abcd6e_i64, 65_u64)
 let j = i64.rotatedRight(0x0123456789abcd6e_i64, 65_u64)
 print("i64 ${a}/${b}/${c}/${d} 8553414939923104896/${e} ${g} ${h}/${j}")
}
fn row7() {
 let a = u64.countOnes(0xfedcba9876543210_u64)
 let b = u64.countZeros(0xfedcba9876543210_u64)
 let c = u64.countLeadingZeros(0xfedcba9876543210_u64)
 let d = u64.countTrailingZeros(0xfedcba9876543210_u64)
 let e = u64.reversedBits(0xfedcba9876543210_u64)
 let f = u64.reversedBytes(0xfedcba9876543210_u64)
 let h = u64.rotatedLeft(0xfedcba9876543210_u64, 65_u64)
 let i = u64.rotatedRight(0xfedcba9876543210_u64, 65_u64)
 print("u64 ${a}/${b}/${c}/${d} ${e} ${f} ${h}/${i}")
}
entry {
 row0()
 row1()
 row2()
 row3()
 row4()
 row5()
 row6()
 row7()
}
