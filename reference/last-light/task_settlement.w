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
  let primary = async readMenuMirror(request: take primaryRequest)
  let fallback = spawn<.network> readMenuMirror(request: take fallbackRequest)
  let candidates: [Task<MirroredMenu, MenuMirrorError>; 2] = [primary, fallback]
  return await (take candidates).firstSettled()
}
