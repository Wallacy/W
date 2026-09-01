// First-settled structured tasks for the Last Light restaurant.

export enum MenuMirror {
  primary
  fallback
}

export struct MenuMirrorRequest {
  mirror: MenuMirror
  revision: u64
}

export struct MirroredMenu {
  mirror: MenuMirror
  revision: u64
}

export enum MenuMirrorError: Error {
  unavailable(mirror: MenuMirror)
}

async fn readMenuMirror(
  request: take MenuMirrorRequest,
): MirroredMenu throws MenuMirrorError {
  guard request.revision > 0 else {
    throw .unavailable(mirror: request.mirror)
  }

  return MirroredMenu(
    mirror: request.mirror,
    revision: request.revision,
  )
}

export async fn firstMenuMirror(
  primaryRequest: take MenuMirrorRequest,
  fallbackRequest: take MenuMirrorRequest,
): TaskSettlement<MirroredMenu, MenuMirrorError> {
  let primary = async readMenuMirror(take primaryRequest)
  let fallback = spawn<.network> readMenuMirror(take fallbackRequest)
  let settlement = await Task#firstSettled(take [primary, fallback])

  return switch take settlement {
    case .some(let winner): take winner
    case .none: panic("two menu mirrors cannot form an empty selection")
  }
}
