# Wire types

| Type | Encoding |
|---|---|
| `u8`, `u16`, `u32` | Unsigned integers, little-endian for multi-byte values. |
| `varuint32` | Up to five bytes, seven payload bits per byte, high bit continues. The fifth byte cannot set its high four bits. Nonminimal encodings are accepted. |
| `f32` | IEEE-754 binary32, little-endian. |
| `raw` | Exactly `extent` uninterpreted bytes. |
| `bytes_u8`, `bytes_u16` | Byte length prefix followed by that many bytes. |
| `guid_compact` | Twelve raw bytes followed by a varuint32. Not interchangeable with a raw sixteen-byte GUID. |
| `optional_u8` | A presence byte followed by one nested structure when nonzero. Not an array count. |
| `array_u8`, `array_u16` | U8 or little-endian U16 count followed by nested elements, bounded by `extent`. |
| `utf16z` | Little-endian UTF-16 units terminated by zero. String limits are specified per field. |
