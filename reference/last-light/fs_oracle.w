// Capability-scoped archive files for the Restaurant at the End of the Universe.

import * from std.fs
import * from std.io

export enum RecipeArchiveError: Error {
  io(IoError)
  snapshot(SnapshotError)
  namespace(NamespaceError)
  publishedButNotDurable(IoError)
}

export async fn recipeArchiveRoot(
  files: ref FileSystem,
  directory: ref Path,
): FileSystem throws IoError {
  return try await files.scope(at: directory)
}

export async fn snapshotRecipe(
  files: ref FileSystem,
  path: ref Path,
  maximumBytes: u64,
): FileSnapshot throws RecipeArchiveError {
  let archive: File<[.read]>

  do {
    archive = try await files.open<[.read]>(path)
  } catch error {
    throw .io(error)
  }

  do {
    return try await archive.snapshot(maximumBytes: maximumBytes)
  } catch error {
    throw .snapshot(error)
  }
}

export async fn extinctRecipeEntries(
  files: ref FileSystem,
  directory: ref Path,
  maximumEntries: usize<(1...)>,
): some Stream<DirectoryEntry, DirectoryError> throws DirectoryError {
  return try await files.entries(
    at: directory,
    limits: DirectoryLimits(
      maximumEntries: maximumEntries,
      maximumNameUnits: 4_096,
      maximumTotalNameUnits: 1_048_576,
    ),
  )
}

export async fn stageRecipe(
  files: ref FileSystem,
  temporaryPath: ref Path,
  payload: view Bytes,
): File<[.write]> throws IoError {
  let staged = try await files.open<[.write]>(
    temporaryPath,
    creation: .replace,
  )
  var committed: usize = 0

  while committed < payload.count {
    let remaining: view Bytes = payload[committed...]

    switch try await staged.write(
      at: FileOffset(committed),
      source: remaining,
    ) {
      case .complete:
        committed = payload.count
      case .partial(let count):
        committed += count
    }
  }

  return staged
}

export async fn publishStagedRecipe(
  files: ref FileSystem,
  staged: take File<[.write]>,
  temporaryPath: ref Path,
  finalPath: ref Path,
  parentDirectory: ref Path,
): () throws RecipeArchiveError {
  do {
    try await (take staged).finish(durability: .data)
  } catch error {
    throw .io(error)
  }

  do {
    try await files.rename(
      from: temporaryPath,
      to: finalPath,
      replacement: .replaceFile,
    )
  } catch error {
    throw .namespace(error)
  }

  do {
    try await files.syncNamespace(at: parentDirectory)
  } catch error {
    throw .publishedButNotDurable(error)
  }
}

test "filesystem policies remain explicit" {
  expect FileCreation.openExisting != FileCreation.replace
  expect RenamePolicy.keepExisting != RenamePolicy.replaceFile
  expect Durability.data != Durability.all
}

// Compile-fail assays:
// let archive = try await files.open<[.read]>(path)
// try await archive.write(at: FileOffset(0), source: payload)
//
// let sharedArchive: shared File<[.read]> = take archive
// let cursor = (take sharedArchive).reader(from: FileOffset(0))
//
// let journal = try await files.open<[.write]>(path)
// try await journal.sync(.all)
// try await journal.syncNamespace(at: parent) // Namespace authority remains on FileSystem.
