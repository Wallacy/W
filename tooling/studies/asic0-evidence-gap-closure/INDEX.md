# ASIC0 index

| Original decision | Current route | Adversarial route | Planned gap |
|---|---|---|---|
| W-1355 | Bounded mapped channel adapter/provider | Unsupported atomic width or hidden provider state | W-1448 |
| W-1359 | Provider-authoritative publication and durability selector receipt | Durable requirement without provider evidence | W-1448 |
| W-1420 | Typed availability provider binding | Raw version evidence | W-1449 |
| W-1425 | Typed runtime composition | Runtime feature grants capability | W-1449 |
| W-1435 | Profile, side-channel, patch, and deployment evidence contracts | Forged receipt or caller echo | W-1450 |

The ten primary cases in the root corpus reference existing IPC1, AVF0, and
SEC0 case IDs. Supplemental `evidenceCaseIds` retain immutable publish,
channel, receipt, fallback, side-channel, patch, supply-chain, and rejected
authority boundaries without copying payloads. The root checker derives every
result through the existing host machines.
The manifest pins all local and reused artifacts by SHA-256.

Current evidence is host-oracle and source-derived. Missing compiler, runtime,
provider, target, hardware, fault, stress, and study evidence remains in each
source study and in the ASIC0 gap map.
