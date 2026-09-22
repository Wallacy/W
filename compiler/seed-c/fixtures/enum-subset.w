enum Stage {
  accepted
  reserving
  preparing
  serving
  completed
}

alias WorkStage = Stage<[.serving, .preparing]>

fn label(stage: WorkStage): i64 {
  return switch stage {
    case .serving: 2
    case .preparing: 1
  }
}

entry {
  let preparing = label(stage: .preparing)
  let serving = label(stage: .serving)
  print("Work ${preparing}/${serving}")
}
