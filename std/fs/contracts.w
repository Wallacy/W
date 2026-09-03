// Capability-scoped native filesystem contracts.
//
// The provider remains missing. FileSystem is a rooted authority, Path keeps
// host-native text, and File rights are checked from their static list.

import * from std.io

export type FileOffset = u64

export enum FileRight: Copy & Equatable & Hashable {
  read
  write
  append
}

export enum FileCreation: Copy & Equatable {
  openExisting
  create
  createNew
  replace
}

export enum Durability: Copy & Equatable {
  none
  data
  all
}

export enum RenamePolicy: Copy & Equatable {
  keepExisting
  replaceFile
}

export enum FileKind: Copy & Equatable {
  regular
  directory
  symbolicLink
  other
}

export enum PathEncodingError: Error & Duplicable {
  invalidUnixUtf8(byteOffset: usize)
  unpairedWindowsUnit(unitOffset: usize)
  nulUnsupported(unitOffset: usize)
}

export enum SnapshotError: Error & Duplicable {
  io(IoError)
  limitExceeded(maximumBytes: u64)
  changedDuringRead
  unsupported
}

export enum DirectoryError: Error & Duplicable {
  io(IoError)
  entryLimitExceeded(maximumEntries: usize)
  nameLimitExceeded(maximumNameUnits: usize)
  totalNameLimitExceeded(maximumTotalNameUnits: usize)
}

export enum NamespaceError: Error & Duplicable {
  io(IoError)
  alreadyExists
  crossMount
  replacementNotRegular
  unknownOutcome(IoError)
}

export struct DirectoryLimits: Copy & Equatable {
  export maximumEntries: usize<(1...)>
  export maximumNameUnits: usize<(1...)>
  export maximumTotalNameUnits: usize<(1...)>

  export init(
    maximumEntries: usize<(1...)> = 4_096,
    maximumNameUnits: usize<(1...)> = 4_096,
    maximumTotalNameUnits: usize<(1...)> = 1_048_576,
  ) {
    self.maximumEntries = maximumEntries
    self.maximumNameUnits = maximumNameUnits
    self.maximumTotalNameUnits = maximumTotalNameUnits
  }
}

foreign intrinsic from "std.fs@1" {
  type PathHandle
  type FileSystemHandle
  type FileHandle
  type FileCursorHandle
  type FileSnapshotHandle
  type DirectoryStreamHandle

  fn stdFsPathFromNative(_ value: ref OsString): PathHandle throws PathEncodingError
  fn stdFsPathFromUtf8(_ value: ref String): PathHandle throws PathEncodingError
  fn stdFsPathFromValidatedUtf8(_ value: ref String): PathHandle
  fn stdFsPathToUtf8(_ handle: ref PathHandle): String throws PathEncodingError
  fn stdFsPathDisplayLossy(_ handle: ref PathHandle): String
  fn stdFsPathDuplicate(_ handle: ref PathHandle): PathHandle
  fn stdFsPathDrop(_ handle: inout PathHandle)

  async fn stdFsScope(
    _ filesystem: ref FileSystemHandle,
    _ path: ref PathHandle,
  ): FileSystemHandle throws IoError
  async fn stdFsOpen<_ rights: StaticList<FileRight>>(
    _ filesystem: ref FileSystemHandle,
    _ path: ref PathHandle,
    _ creation: FileCreation,
  ): FileHandle throws IoError
  async fn stdFsMetadata(
    _ filesystem: ref FileSystemHandle,
    _ path: ref PathHandle,
  ): FileMetadata throws IoError
  async fn stdFsEntries(
    _ filesystem: ref FileSystemHandle,
    _ path: ref PathHandle,
    _ limits: DirectoryLimits,
  ): DirectoryStreamHandle throws DirectoryError
  async fn stdFsCreateDirectory(
    _ filesystem: ref FileSystemHandle,
    _ path: ref PathHandle,
  ): () throws NamespaceError
  async fn stdFsRemoveFile(
    _ filesystem: ref FileSystemHandle,
    _ path: ref PathHandle,
  ): () throws NamespaceError
  async fn stdFsRemoveEmptyDirectory(
    _ filesystem: ref FileSystemHandle,
    _ path: ref PathHandle,
  ): () throws NamespaceError
  async fn stdFsRename(
    _ filesystem: ref FileSystemHandle,
    _ source: ref PathHandle,
    _ destination: ref PathHandle,
    _ replacement: RenamePolicy,
  ): () throws NamespaceError
  async fn stdFsSyncNamespace(
    _ filesystem: ref FileSystemHandle,
    _ path: ref PathHandle,
  ): () throws IoError
  fn stdFsFileSystemDrop(_ handle: inout FileSystemHandle)

  async fn stdFsFileRead(
    _ handle: ref FileHandle,
    _ offset: FileOffset,
    _ destination: inout Bytes,
    _ maximum: usize<(1...)>,
  ): ReadStep throws IoError
  async fn stdFsFileWrite(
    _ handle: ref FileHandle,
    _ offset: FileOffset,
    _ source: view Bytes,
  ): WriteStep throws IoError
  async fn stdFsFileAppend(
    _ handle: ref FileHandle,
    _ source: view Bytes,
  ): WriteStep throws IoError
  fn stdFsFileReader(
    _ handle: take FileHandle,
    _ offset: FileOffset,
  ): FileCursorHandle
  async fn stdFsFileSnapshot(
    _ handle: ref FileHandle,
    _ maximumBytes: u64,
  ): FileSnapshotHandle throws SnapshotError
  async fn stdFsFileSync(
    _ handle: ref FileHandle,
    _ durability: Durability,
  ): () throws IoError
  async fn stdFsFileFinish(
    _ handle: take FileHandle,
    _ durability: Durability,
  ): () throws IoError
  fn stdFsFileDrop(_ handle: inout FileHandle)

  async fn stdFsCursorRead(
    _ handle: inout FileCursorHandle,
    _ destination: inout Bytes,
    _ maximum: usize<(1...)>,
  ): ReadStep throws IoError
  fn stdFsCursorDrop(_ handle: inout FileCursorHandle)

  fn stdFsSnapshotByteCount(_ handle: ref FileSnapshotHandle): u64
  async fn stdFsSnapshotRead(
    _ handle: ref FileSnapshotHandle,
    _ offset: u64,
    _ destination: inout Bytes,
    _ maximum: usize<(1...)>,
  ): SnapshotReadStep throws IoError
  fn stdFsSnapshotDrop(_ handle: inout FileSnapshotHandle)

  async fn stdFsDirectoryNext(
    _ handle: inout DirectoryStreamHandle,
  ): DirectoryEntry? throws DirectoryError
  async fn stdFsDirectoryCancel(
    _ handle: inout DirectoryStreamHandle,
  ): () throws DirectoryError
  fn stdFsDirectoryDrop(_ handle: inout DirectoryStreamHandle)
}

export struct Path: Duplicable {
  handle: PathHandle

  export init(native: ref OsString) throws PathEncodingError {
    self.handle = unsafe { try stdFsPathFromNative(native) }
  }

  init(validatedHandle: PathHandle) {
    self.handle = validatedHandle
  }

  export static fn fromUtf8(path: ref Utf8Path): Path {
    return Path(validatedHandle: unsafe { stdFsPathFromValidatedUtf8(path.text()) })
  }

  export fn toUtf8(): Utf8Path throws PathEncodingError {
    let text = unsafe { try stdFsPathToUtf8(ref handle) }
    return Utf8Path(validatedText: take text)
  }

  export fn displayLossy(): String {
    return unsafe { stdFsPathDisplayLossy(ref handle) }
  }

  export fn duplicate(): Path {
    return Path(validatedHandle: unsafe { stdFsPathDuplicate(ref handle) })
  }

  deinit {
    unsafe { stdFsPathDrop(inout handle) }
  }
}

export struct Utf8Path: Duplicable {
  value: String

  export init(text: String) throws PathEncodingError {
    let validated = unsafe { try stdFsPathFromUtf8(ref text) }
    unsafe { stdFsPathDrop(inout validated) }
    self.value = take text
  }

  export init(path: ref Path) throws PathEncodingError {
    self.value = unsafe { try stdFsPathToUtf8(ref path.handle) }
  }

  init(validatedText: String) {
    self.value = take validatedText
  }

  export fn text(): ref String {
    return ref value
  }

  export fn duplicate(): Utf8Path {
    return Utf8Path(validatedText: copy value)
  }
}

export struct FileMetadata {
  kindValue: FileKind
  byteLengthValue: u64?

  init(kind: FileKind, byteLength: u64?) {
    self.kindValue = kind
    self.byteLengthValue = byteLength
  }

  export kind: FileKind {
    get => kindValue
  }

  export byteLength: u64? {
    get => byteLengthValue
  }
}

export struct DirectoryEntry {
  nameValue: OsString
  kindHintValue: FileKind?

  init(name: OsString, kindHint: FileKind?) {
    self.nameValue = take name
    self.kindHintValue = kindHint
  }

  export name: ref OsString {
    get => ref nameValue
  }

  export kindHint: FileKind? {
    get => kindHintValue
  }
}

export struct FileSnapshot: SnapshotByteSource<IoError> {
  handle: FileSnapshotHandle

  init(validatedHandle: FileSnapshotHandle) {
    self.handle = validatedHandle
  }

  export fn byteCount(): u64 {
    return unsafe { stdFsSnapshotByteCount(ref handle) }
  }

  export async fn read(
    at offset: u64,
    appendTo destination: inout Bytes,
    maximum: usize<(1...)>,
  ): SnapshotReadStep throws IoError {
    return unsafe {
      try await stdFsSnapshotRead(
        ref handle,
        offset,
        inout destination,
        maximum,
      )
    }
  }

  deinit {
    unsafe { stdFsSnapshotDrop(inout handle) }
  }
}

struct DirectoryStream: Stream<DirectoryEntry, DirectoryError> {
  handle: DirectoryStreamHandle

  init(validatedHandle: DirectoryStreamHandle) {
    self.handle = validatedHandle
  }

  mut async fn next(): DirectoryEntry? throws DirectoryError {
    return unsafe { try await stdFsDirectoryNext(inout handle) }
  }

  take async fn cancel(): () throws DirectoryError {
    unsafe { try await stdFsDirectoryCancel(inout handle) }
  }

  deinit {
    unsafe { stdFsDirectoryDrop(inout handle) }
  }
}

struct FileCursor: ByteSource<IoError> {
  handle: FileCursorHandle

  init(validatedHandle: FileCursorHandle) {
    self.handle = validatedHandle
  }

  mut async fn read(
    appendTo destination: inout Bytes,
    maximum: usize<(1...)>,
  ): ReadStep throws IoError {
    return unsafe {
      try await stdFsCursorRead(
        inout handle,
        inout destination,
        maximum,
      )
    }
  }

  deinit {
    unsafe { stdFsCursorDrop(inout handle) }
  }
}

export struct File<_ rights: StaticList<FileRight>> {
  handle: FileHandle

  init(validatedHandle: FileHandle) {
    self.handle = validatedHandle
  }

  // S0 exposes this member only when rights contains .read.
  export async fn read(
    at offset: FileOffset,
    appendTo destination: inout Bytes,
    maximum: usize<(1...)>,
  ): ReadStep throws IoError {
    return unsafe {
      try await stdFsFileRead(
        ref handle,
        offset,
        inout destination,
        maximum,
      )
    }
  }

  // S0 exposes this member only when rights contains .write.
  export async fn write(
    at offset: FileOffset,
    source: view Bytes,
  ): WriteStep throws IoError {
    return unsafe { try await stdFsFileWrite(ref handle, offset, source) }
  }

  // S0 exposes this member only when rights contains .append.
  export async fn append(source: view Bytes): WriteStep throws IoError {
    return unsafe { try await stdFsFileAppend(ref handle, source) }
  }

  // S0 exposes this member only when rights contains .read.
  export take fn reader(from offset: FileOffset): some ByteSource<IoError> {
    let cursor = unsafe { stdFsFileReader(take handle, offset) }
    return FileCursor(validatedHandle: cursor)
  }

  // S0 exposes this member only when rights contains .read.
  export async fn snapshot(
    maximumBytes: u64,
  ): FileSnapshot throws SnapshotError {
    let snapshot = unsafe {
      try await stdFsFileSnapshot(ref handle, maximumBytes)
    }
    return FileSnapshot(validatedHandle: snapshot)
  }

  // S0 exposes sync only for .write or .append.
  export async fn sync(durability: Durability): () throws IoError {
    if durability == .none { return }
    unsafe { try await stdFsFileSync(ref handle, durability) }
  }

  // S0 exposes nontrivial durability only for .write or .append.
  export take async fn finish(
    durability: Durability = .none,
  ): () throws IoError {
    unsafe { try await stdFsFileFinish(take handle, durability) }
  }

  deinit {
    unsafe { stdFsFileDrop(inout handle) }
  }
}

export struct FileSystem {
  handle: FileSystemHandle

  init(validatedHandle: FileSystemHandle) {
    self.handle = validatedHandle
  }

  export async fn scope(at path: ref Path): FileSystem throws IoError {
    let child = unsafe { try await stdFsScope(ref handle, ref path.handle) }
    return FileSystem(validatedHandle: child)
  }

  export async fn open<_ rights: StaticList<FileRight>>(
    path: ref Path,
    creation: FileCreation = .openExisting,
  ): File<rights> throws IoError {
    let file = unsafe {
      try await stdFsOpen<rights>(ref handle, ref path.handle, creation)
    }
    return File<rights>(validatedHandle: file)
  }

  export async fn metadata(path: ref Path): FileMetadata throws IoError {
    return unsafe { try await stdFsMetadata(ref handle, ref path.handle) }
  }

  export async fn entries(
    at path: ref Path,
    limits: DirectoryLimits = DirectoryLimits(),
  ): some Stream<DirectoryEntry, DirectoryError> throws DirectoryError {
    let rawStream = unsafe {
      try await stdFsEntries(ref handle, ref path.handle, limits)
    }
    return DirectoryStream(validatedHandle: rawStream)
  }

  export async fn createDirectory(path: ref Path): () throws NamespaceError {
    unsafe { try await stdFsCreateDirectory(ref handle, ref path.handle) }
  }

  export async fn removeFile(path: ref Path): () throws NamespaceError {
    unsafe { try await stdFsRemoveFile(ref handle, ref path.handle) }
  }

  export async fn removeEmptyDirectory(
    path: ref Path,
  ): () throws NamespaceError {
    unsafe { try await stdFsRemoveEmptyDirectory(ref handle, ref path.handle) }
  }

  export async fn rename(
    from source: ref Path,
    to destination: ref Path,
    replacement: RenamePolicy = .keepExisting,
  ): () throws NamespaceError {
    unsafe {
      try await stdFsRename(
        ref handle,
        ref source.handle,
        ref destination.handle,
        replacement,
      )
    }
  }

  export async fn syncNamespace(
    at path: ref Path,
  ): () throws IoError {
    unsafe { try await stdFsSyncNamespace(ref handle, ref path.handle) }
  }

  deinit {
    unsafe { stdFsFileSystemDrop(inout handle) }
  }
}

test "file rights remain distinct static values" {
  let rights = [FileRight.read, FileRight.write, FileRight.append]
  expect rights.count == 3
  expect rights[0] != rights[1]
}

test "namespace replacement is explicit" {
  expect RenamePolicy.keepExisting != RenamePolicy.replaceFile
}

test "durability levels remain distinct" {
  expect Durability.none != Durability.data
  expect Durability.data != Durability.all
}
