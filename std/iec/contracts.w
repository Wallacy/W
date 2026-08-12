// IEC source projection for information units.
//
// Unit names are exported bindings. A caller must import a name or use the
// module binding (`iec.KiB`); there is no ambient unit registry. The reference
// unit for Information is bit. Binary prefixes use exact powers of 1024.
// Declarations are ordered from the dimension and reference unit to derived
// units. Reference resolution remains declaration-order independent.

export dimension Information
export unit bit: Information
export unit byte = 8<bit>
export unit KiB = 1_024<byte>
export unit MiB = 1_024<KiB>
export unit GiB = 1_024<MiB>
