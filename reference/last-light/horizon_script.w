// PYN1 single-file design oracle. The host does not claim execution.
// Its map names the canonical roots, imports, lock digest, CAS, authority,
// signature, offline, network, target, standalone, failure, provenance, graph,
// cleanup, and promotion gates.
// `w context`, `w script add`, `w script remove`, and `w script resolve` edit
// only the data header. PATH, environment, stdin, URL, shebang, hidden state, and transitive
// capability grants remain rejected. The default entry is unnamed. The oracle
// also covers lock roots, nodes/edges closure, artifact evidence, CAS, action output, handles, local override, ambient
// registry, traversal, symlink, case, drive, UNC, requirements, process args,
// `requires`, `.path`, branch, recursive, secrets, atomic, and local graph
// boundaries. `parseEvidence` and `resultParseEvidence` bind the parser
// projection to source bytes; these words name adversarial symbols and do not claim that the
// source executes them.

script {
  edition: "2026"
  dependencies: [
    {
      alias: "chart"
      package: "fiction/chart"
      version: "^1.2.0"
      use: .product
      source: .registry("w")
    },
  ]
  lock: "sha256:f59a22a26aa53fc0d1555350c177b8013d2f1532554861872ff87f94ab0e8cf2"
}

module horizon_script

import chart.science
import { Batch } from std.data

export struct HorizonReading {
  sequence: u64
  flux: f64
}

fn horizonScore(batch: Batch<HorizonReading>): f64 {
  if batch.rowCount() == 0 {
    return 0.0
  }
  return chart.science.score(batch)
}

fn menuFor(score: f64): String {
  if score >= 0.85 {
    return "evacuation"
  }
  if score >= 0.55 {
    return "warning"
  }
  return "steady"
}

fn provenance(score: f64, label: String): String {
  return "horizon:" + label
}

entry {
  let batch: Batch<HorizonReading> = chart.science.sample()
  let score = horizonScore(batch)
  let label = menuFor(score)
  let result = provenance(score, label)
  print(result)
}
