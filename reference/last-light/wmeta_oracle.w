// Physical oracle for WMeta1 metadata.
//
// Platform containers preserve these bytes. They do not change this format.
// The host reference readers validate the exact header, directory, and chunks.

export const W_META_HEADER_BYTES: u16 = 32
export const W_META_DIRECTORY_SCHEMA: u16 = 1
export const W_META_SHA256: u16 = 1

export enum WMetaProfile {
  interface
  objectAbi
}

export enum WMetaChunkKind {
  interfaceIndex
  semanticInterface
  documentation
  diagnosticMap
  genericBody
  abiNote
  representationMap
  symbolManifest
  runtimeRequirements
  extension
}

export enum WMetaOpenMode {
  directory
  core
  full
}

export struct WMetaHeader {
  headerBytes: u16
  directorySchema: u16
  flags: u32
  directoryBytes: u64
  payloadBytes: u64
}

export struct WMetaChunkEntry {
  kind: WMetaChunkKind
  identity: Bytes
  schemaMajor: u16
  schemaMinor: u16
  critical: Bool
  length: u64
  digestAlgorithm: u16
  digest: Bytes
}

export const fn wmetaTotalBytes(header: ref WMetaHeader): u64 {
  return W_META_HEADER_BYTES + header.directoryBytes + header.payloadBytes
}

// Chunk offsets are prefix sums. A directory entry cannot provide an offset.
export const fn nextChunkOffset(offset: u64, entry: ref WMetaChunkEntry): u64 {
  return offset + entry.length
}

export const fn profileAccepts(
  profile: WMetaProfile,
  kind: WMetaChunkKind,
): Bool {
  return switch profile {
    case .interface:
      kind.one(
        .interfaceIndex,
        .semanticInterface,
        .documentation,
        .diagnosticMap,
        .genericBody,
        .extension,
      )
    case .objectAbi:
      kind.one(
        .abiNote,
        .representationMap,
        .symbolManifest,
        .runtimeRequirements,
        .extension,
      )
  }
}

export const fn profileRequires(
  profile: WMetaProfile,
  kind: WMetaChunkKind,
): Bool {
  return switch profile {
    case .interface: kind.one(.interfaceIndex, .semanticInterface)
    case .objectAbi:
      kind.one(.abiNote, .representationMap, .symbolManifest, .runtimeRequirements)
  }
}

export const fn openModeReads(mode: WMetaOpenMode, critical: Bool): Bool {
  return switch mode {
    case .directory: false
    case .core: critical
    case .full: true
  }
}

test "WMeta profiles close their core chunks" for profileRequires {
  expect profileRequires(profile: .interface, kind: .interfaceIndex)
  expect profileRequires(profile: .interface, kind: .semanticInterface)
  expect !profileRequires(profile: .interface, kind: .documentation)
  expect profileRequires(profile: .objectAbi, kind: .abiNote)
  expect profileRequires(profile: .objectAbi, kind: .representationMap)
  expect profileRequires(profile: .objectAbi, kind: .symbolManifest)
  expect profileRequires(profile: .objectAbi, kind: .runtimeRequirements)
}

test "WMeta open modes do not publish partial core state" for openModeReads {
  expect !openModeReads(mode: .directory, critical: true)
  expect openModeReads(mode: .core, critical: true)
  expect !openModeReads(mode: .core, critical: false)
  expect openModeReads(mode: .full, critical: false)
}
