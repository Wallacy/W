import { createHash } from "node:crypto";

export class PackageReleaseError extends Error {
  constructor(code) {
    super(code);
    this.code = code;
  }
}

function canonical(value) {
  if (Array.isArray(value)) return value.map(canonical);
  if (value && typeof value === "object") {
    return Object.fromEntries(
      Object.keys(value)
        .sort()
        .map((key) => [key, canonical(value[key])]),
    );
  }
  return value;
}

function digest(tag, value) {
  const bytes = typeof value === "string" ? value : JSON.stringify(canonical(value));
  return `sha256:${createHash("sha256").update(`${tag}\0${bytes}`).digest("hex")}`;
}

function parseVersion(version) {
  const match = /^(\d+)\.(\d+)\.(\d+)$/.exec(version);
  if (!match) throw new PackageReleaseError("unsupportedVersionSyntax");
  return match.slice(1).map(Number);
}

function compareVersions(left, right) {
  const a = parseVersion(left);
  const b = parseVersion(right);
  for (let index = 0; index < 3; index += 1) {
    if (a[index] !== b[index]) return a[index] - b[index];
  }
  return 0;
}

function satisfies(version, constraint) {
  if (constraint === "*") return true;
  if (constraint.startsWith("^")) {
    const current = parseVersion(version);
    const base = parseVersion(constraint.slice(1));
    if (compareVersions(version, constraint.slice(1)) < 0) return false;
    if (base[0] > 0) return current[0] === base[0];
    if (base[1] > 0) return current[0] === 0 && current[1] === base[1];
    return current[0] === 0 && current[1] === 0 && current[2] === base[2];
  }
  const exact = constraint.startsWith("=") ? constraint.slice(1) : constraint;
  parseVersion(exact);
  return version === exact;
}

function requireSnapshot(state, name) {
  const snapshot = state.snapshots[name];
  if (!snapshot) throw new PackageReleaseError("unknownRegistrySnapshot");
  return snapshot;
}

function requireRealm(state, name) {
  const realm = state.realms[name];
  if (!realm) throw new PackageReleaseError("unknownResolutionRealm");
  return realm;
}

function requireLock(state, name) {
  const lock = state.locks[name];
  if (!lock) throw new PackageReleaseError("unknownPackageLock");
  return lock;
}

function requireRecipe(state, name) {
  const recipe = state.recipes[name];
  if (!recipe) throw new PackageReleaseError("unknownRecipe");
  return recipe;
}

function requireBuild(state, name) {
  const build = state.builds[name];
  if (!build) throw new PackageReleaseError("unknownBuildEvidence");
  return build;
}

function requireRelease(state, name) {
  const release = state.releases[name];
  if (!release) throw new PackageReleaseError("unknownRelease");
  return release;
}

function normalizedCandidate(candidate) {
  parseVersion(candidate.version);
  return {
    identity: candidate.identity,
    version: candidate.version,
    source: candidate.source,
    contentTreeDigest: digest("w-source-tree-v1", candidate.content),
    yanked: candidate.yanked ?? false,
    revoked: candidate.revoked ?? false,
    dependencies: (candidate.dependencies ?? [])
      .map((dependency) => ({
        identity: dependency.identity,
        constraint: dependency.constraint,
        features: [...new Set(dependency.features ?? [])].sort(),
      }))
      .sort((left, right) =>
        `${left.identity}\0${left.constraint}`.localeCompare(
          `${right.identity}\0${right.constraint}`,
        ),
      ),
  };
}

function candidateKey(candidate) {
  return `${candidate.identity}@${candidate.version}`;
}

function lockContextKey(context) {
  return [context.realm, context.root, context.target, context.use].join("|");
}

function normalizeInventoryMap(inventories) {
  return canonical(
    Object.fromEntries(
      Object.entries(inventories).map(([identity, sources]) => [
        identity,
        [...sources].sort(),
      ]),
    ),
  );
}

function addConstraint(constraints, identity, constraint, features, from) {
  const next = new Map(constraints);
  const current = next.get(identity) ?? [];
  next.set(identity, [...current, { constraint, features, from }]);
  return next;
}

function compatibleCandidates(snapshot, identity, requirements, conflicts) {
  const all = snapshot.candidates.filter((candidate) => candidate.identity === identity);
  const members = all.filter((candidate) => candidate.source === "member");
  const pool = members.length > 0 ? members : all.filter((candidate) => candidate.source === "registry");
  const compatible = pool.filter(
    (candidate) =>
      !candidate.revoked &&
      !candidate.yanked &&
      requirements.every((requirement) => satisfies(candidate.version, requirement.constraint)),
  );
  if (members.length > 0 && compatible.length === 0) {
    conflicts.workspaceMember = true;
  }
  return compatible.sort((left, right) => {
    const versionOrder = compareVersions(right.version, left.version);
    if (versionOrder !== 0) return versionOrder;
    return left.contentTreeDigest.localeCompare(right.contentTreeDigest);
  });
}

function solveSnapshot(snapshot, roots) {
  const conflicts = { workspaceMember: false };
  let constraints = new Map();
  for (const root of roots) {
    constraints = addConstraint(
      constraints,
      root.identity,
      root.constraint,
      root.features,
      root.from,
    );
  }

  function search(currentConstraints, selections) {
    for (const [identity, requirements] of currentConstraints) {
      const selected = selections.get(identity);
      if (
        selected &&
        !requirements.every((requirement) => satisfies(selected.version, requirement.constraint))
      ) {
        return null;
      }
    }
    const unresolved = [...currentConstraints.keys()]
      .filter((identity) => !selections.has(identity))
      .sort()[0];
    if (!unresolved) return { constraints: currentConstraints, selections };

    const candidates = compatibleCandidates(
      snapshot,
      unresolved,
      currentConstraints.get(unresolved),
      conflicts,
    );
    for (const candidate of candidates) {
      const nextSelections = new Map(selections);
      nextSelections.set(unresolved, candidate);
      let nextConstraints = new Map(currentConstraints);
      for (const dependency of candidate.dependencies) {
        nextConstraints = addConstraint(
          nextConstraints,
          dependency.identity,
          dependency.constraint,
          dependency.features,
          `${candidate.identity}@${candidate.version}`,
        );
      }
      const result = search(nextConstraints, nextSelections);
      if (result) return result;
    }
    return null;
  }

  const result = search(constraints, new Map());
  if (!result && conflicts.workspaceMember) {
    throw new PackageReleaseError("workspaceMemberVersionConflict");
  }
  if (!result) throw new PackageReleaseError("resolutionConflict");
  return [...result.selections.entries()]
    .sort(([left], [right]) => left.localeCompare(right))
    .map(([identity, candidate]) => ({
      identity,
      version: candidate.version,
      source: candidate.source,
      contentTreeDigest: candidate.contentTreeDigest,
      features: [
        ...new Set(
          (result.constraints.get(identity) ?? []).flatMap((requirement) => requirement.features),
        ),
      ].sort(),
      reasons: (result.constraints.get(identity) ?? [])
        .map((requirement) => `${requirement.from}:${requirement.constraint}`)
        .sort(),
    }));
}

function independent(left, right) {
  return (
    left.builderIdentity !== right.builderIdentity &&
    left.operatorIdentity !== right.operatorIdentity &&
    left.credentialIdentity !== right.credentialIdentity &&
    left.executionRootIdentity !== right.executionRootIdentity
  );
}

function maximumIndependentCount(builds) {
  let maximum = 0;
  function visit(index, selected) {
    if (index === builds.length) {
      maximum = Math.max(maximum, selected.length);
      return;
    }
    visit(index + 1, selected);
    if (selected.every((candidate) => independent(candidate, builds[index]))) {
      visit(index + 1, [...selected, builds[index]]);
    }
  }
  visit(0, []);
  return maximum;
}

function roleCollision(release, builds) {
  const authorities = [
    release.maintainerIdentity,
    release.registryIdentity,
    release.toolchainProviderIdentity,
    release.platformSignerIdentity,
  ];
  if (new Set(authorities).size !== authorities.length) return true;
  return builds.some((build) => authorities.includes(build.builderIdentity));
}

function applyOperation(state, operation) {
  switch (operation.op) {
    case "acceptRegistrySnapshot": {
      if (!operation.thresholdValid) throw new PackageReleaseError("metadataThresholdInvalid");
      if (!operation.delegationValid) throw new PackageReleaseError("metadataDelegationInvalid");
      if (!operation.fresh) throw new PackageReleaseError("metadataExpired");
      if (operation.version <= state.trustedSnapshotVersion) {
        throw new PackageReleaseError("metadataRollback");
      }
      const candidates = operation.candidates.map(normalizedCandidate).sort((left, right) =>
        `${left.identity}\0${left.version}\0${left.source}\0${left.contentTreeDigest}`.localeCompare(
          `${right.identity}\0${right.version}\0${right.source}\0${right.contentTreeDigest}`,
        ),
      );
      const seen = new Map();
      for (const candidate of candidates) {
        const key = candidateKey(candidate);
        if (seen.has(key) && seen.get(key) !== candidate.contentTreeDigest) {
          throw new PackageReleaseError("registryEquivocation");
        }
        seen.set(key, candidate.contentTreeDigest);
      }
      state.snapshots[operation.snapshot] = {
        version: operation.version,
        digest: digest("w-registry-snapshot-v1", candidates),
        candidates,
      };
      state.trustedSnapshotVersion = operation.version;
      return;
    }
    case "createRealm": {
      if (state.realms[operation.realm]) throw new PackageReleaseError("realmAlreadyExists");
      requireSnapshot(state, operation.snapshot);
      state.realms[operation.realm] = {
        snapshot: operation.snapshot,
        requirements: [],
        selections: null,
      };
      return;
    }
    case "addRequirement": {
      const realm = requireRealm(state, operation.realm);
      parseVersion(operation.constraint.startsWith("^") || operation.constraint.startsWith("=")
        ? operation.constraint.slice(1)
        : operation.constraint === "*"
          ? "0.0.0"
          : operation.constraint);
      realm.requirements.push({
        identity: operation.identity,
        constraint: operation.constraint,
        features: [...new Set(operation.features ?? [])].sort(),
        from: operation.from,
      });
      realm.selections = null;
      return;
    }
    case "solveRealm": {
      const realm = requireRealm(state, operation.realm);
      const snapshot = requireSnapshot(state, realm.snapshot);
      realm.selections = solveSnapshot(snapshot, realm.requirements);
      realm.digest = digest("w-resolution-realm-v1", realm.selections);
      return;
    }
    case "verifySelection": {
      const realm = requireRealm(state, operation.realm);
      if (!realm.selections) throw new PackageReleaseError("realmNotSolved");
      const selected = realm.selections.find((entry) => entry.identity === operation.identity);
      if (!selected) throw new PackageReleaseError("packageNotSelected");
      for (const field of ["version", "source", "contentTreeDigest"]) {
        if (operation[field] !== undefined && selected[field] !== operation[field]) {
          throw new PackageReleaseError("unexpectedPackageSelection");
        }
      }
      if (
        operation.features !== undefined &&
        JSON.stringify(selected.features) !== JSON.stringify(operation.features)
      ) {
        throw new PackageReleaseError("unexpectedPackageFeatures");
      }
      return;
    }
    case "writeLock": {
      if (state.locks[operation.lock]) throw new PackageReleaseError("lockAlreadyWritten");
      const contexts = operation.contexts
        .map((context) => {
          const realm = requireRealm(state, context.realm);
          if (!realm.selections) throw new PackageReleaseError("realmNotSolved");
          return {
            realm: context.realm,
            root: context.root,
            target: context.target,
            use: context.use,
            activeSourceSet: context.activeSourceSet,
            resolutionDigest: realm.digest,
            selections: realm.selections,
          };
        })
        .sort((left, right) =>
          `${left.realm}\0${left.root}\0${left.target}\0${left.use}`.localeCompare(
            `${right.realm}\0${right.root}\0${right.target}\0${right.use}`,
          ),
        );
      if (new Set(contexts.map(lockContextKey)).size !== contexts.length) {
        throw new PackageReleaseError("duplicateLockContext");
      }
      const record = canonical({
        schema: "w.package-lock/1",
        resolver: operation.resolver,
        workspaceDigest: operation.workspaceDigest,
        manifestDigests: operation.manifestDigests,
        sourceInventories: normalizeInventoryMap(operation.sourceInventories),
        contexts,
      });
      state.locks[operation.lock] = {
        record,
        digest: digest("w-package-lock-v1", record),
        validated: false,
      };
      return;
    }
    case "validateLocked": {
      const lock = requireLock(state, operation.lock);
      const record = lock.record;
      if (record.workspaceDigest !== operation.workspaceDigest) {
        throw new PackageReleaseError("lockedWorkspaceMismatch");
      }
      if (JSON.stringify(record.manifestDigests) !== JSON.stringify(canonical(operation.manifestDigests))) {
        throw new PackageReleaseError("lockedManifestMismatch");
      }
      if (
        JSON.stringify(record.sourceInventories) !==
        JSON.stringify(normalizeInventoryMap(operation.sourceInventories))
      ) {
        throw new PackageReleaseError("lockedSourceInventoryMismatch");
      }
      const expectedSets = Object.fromEntries(
        record.contexts.map((context) => [lockContextKey(context), context.activeSourceSet]),
      );
      if (JSON.stringify(expectedSets) !== JSON.stringify(canonical(operation.activeSourceSets))) {
        throw new PackageReleaseError("lockedActiveSourceSetMismatch");
      }
      lock.validated = true;
      return;
    }
    case "verifyLock": {
      const lock = requireLock(state, operation.lock);
      if (operation.validated !== undefined && lock.validated !== operation.validated) {
        throw new PackageReleaseError("unexpectedLockState");
      }
      if (operation.digest !== undefined && lock.digest !== operation.digest) {
        throw new PackageReleaseError("unexpectedLockDigest");
      }
      return;
    }
    case "declareObject": {
      if (state.objects[operation.object]) throw new PackageReleaseError("objectAlreadyDeclared");
      state.objects[operation.object] = {
        kind: operation.kind ?? "blob",
        digest: digest(`w-cas-${operation.kind ?? "blob"}-v1`, operation.content),
        expectedContent: operation.content,
      };
      return;
    }
    case "storeObject": {
      const object = state.objects[operation.object];
      if (!object) throw new PackageReleaseError("unknownDeclaredObject");
      if (digest(`w-cas-${object.kind}-v1`, operation.content) !== object.digest) {
        throw new PackageReleaseError("casDigestMismatch");
      }
      state.cas[object.digest] = operation.content;
      return;
    }
    case "registerMirror": {
      state.mirrors[operation.mirror] = { listed: operation.listed };
      return;
    }
    case "fetchMirror": {
      const mirror = state.mirrors[operation.mirror];
      if (!mirror?.listed) throw new PackageReleaseError("mirrorUnlisted");
      if (!operation.metadataFresh) throw new PackageReleaseError("mirrorMetadataExpired");
      if (operation.metadataVersion < state.trustedSnapshotVersion) {
        throw new PackageReleaseError("mirrorMetadataRollback");
      }
      const object = state.objects[operation.object];
      if (!object) throw new PackageReleaseError("unknownDeclaredObject");
      if (digest(`w-cas-${object.kind}-v1`, operation.content) !== object.digest) {
        throw new PackageReleaseError("mirrorDigestMismatch");
      }
      state.cas[object.digest] = operation.content;
      return;
    }
    case "requireOffline": {
      for (const name of operation.objects) {
        const object = state.objects[name];
        if (!object || state.cas[object.digest] === undefined) {
          throw new PackageReleaseError("offlineObjectMissing");
        }
      }
      return;
    }
    case "createRecipe": {
      if (state.recipes[operation.recipe]) throw new PackageReleaseError("recipeAlreadyCreated");
      const lock = requireLock(state, operation.lock);
      if (!lock.validated) throw new PackageReleaseError("recipeRequiresValidatedLock");
      if (operation.outputs !== undefined) {
        throw new PackageReleaseError("recipeContainsOwnOutput");
      }
      if ((operation.forbiddenInfluences ?? []).length > 0) {
        throw new PackageReleaseError("forbiddenBuildInfluence");
      }
      const allowed = new Set(operation.allowedEnvironment ?? []);
      if (Object.keys(operation.environment ?? {}).some((key) => !allowed.has(key))) {
        throw new PackageReleaseError("undeclaredBuildEnvironment");
      }
      const inputs = operation.inputs
        .map((name) => {
          const object = state.objects[name];
          if (!object) throw new PackageReleaseError("unknownRecipeInput");
          return { name, digest: object.digest };
        })
        .sort((left, right) => left.name.localeCompare(right.name));
      const sourceTrees = operation.sourceTrees
        .map((source) => {
          const object = state.objects[source.object];
          if (!object || object.kind !== "source-tree") {
            throw new PackageReleaseError("unknownRecipeSourceTree");
          }
          return {
            package: source.package,
            digest: object.digest,
          };
        })
        .sort((left, right) => left.package.localeCompare(right.package));
      const record = canonical({
        schema: "w.recipe/1",
        lockDigest: lock.digest,
        product: operation.product,
        target: operation.target,
        profile: operation.profile,
        toolchainPlanDigest: operation.toolchainPlanDigest,
        runtimeClosureDigest: operation.runtimeClosureDigest,
        sourceTrees,
        environment: operation.environment ?? {},
        inputs,
      });
      state.recipes[operation.recipe] = {
        record,
        digest: digest("w-build-recipe-v1", record),
      };
      return;
    }
    case "verifyRecipeRelation": {
      const left = requireRecipe(state, operation.left);
      const right = requireRecipe(state, operation.right);
      const equal = left.digest === right.digest;
      if (equal !== operation.equal) throw new PackageReleaseError("unexpectedRecipeRelation");
      return;
    }
    case "runBuild": {
      if (state.builds[operation.build]) throw new PackageReleaseError("buildAlreadyRecorded");
      const recipe = requireRecipe(state, operation.recipe);
      for (const input of recipe.record.inputs) {
        if (state.cas[input.digest] === undefined) {
          throw new PackageReleaseError("buildInputMissingFromCas");
        }
      }
      for (const source of recipe.record.sourceTrees) {
        if (state.cas[source.digest] === undefined) {
          throw new PackageReleaseError("buildSourceMissingFromCas");
        }
      }
      const payloadDigest = digest("w-payload-v1", operation.payload);
      const artifactRecord = canonical({
        payloadDigest,
        resources: (operation.resources ?? []).map((content) =>
          digest("w-resource-v1", content),
        ),
        sidecars: (operation.sidecars ?? []).map((content) =>
          digest("w-sidecar-v1", content),
        ),
      });
      const artifactDigest = digest("w-artifact-v1", artifactRecord);
      const previous = state.recipeOutputs[recipe.digest];
      if (previous && previous !== artifactDigest) {
        state.conflictingRecipeOutputs[recipe.digest] = [previous, artifactDigest];
        throw new PackageReleaseError("nondeterministicRecipeOutput");
      }
      state.recipeOutputs[recipe.digest] = artifactDigest;
      state.builds[operation.build] = {
        recipe: operation.recipe,
        recipeDigest: recipe.digest,
        lockDigest: recipe.record.lockDigest,
        toolchainDigest: recipe.record.toolchainPlanDigest,
        targetDigest: digest("w-target-v1", recipe.record.target),
        runtimeClosureDigest: recipe.record.runtimeClosureDigest,
        environmentProjectionDigest: digest(
          "w-environment-v1",
          recipe.record.environment,
        ),
        payloadDigest,
        artifactDigest,
        inputsComplete: operation.inputsComplete ?? true,
        outputsComplete: operation.outputsComplete ?? true,
        builderIdentity: operation.builderIdentity,
        operatorIdentity: operation.operatorIdentity,
        credentialIdentity: operation.credentialIdentity,
        executionRootIdentity: operation.executionRootIdentity,
        executorIdentity: operation.executorIdentity,
      };
      return;
    }
    case "verifyBuildRelation": {
      const left = requireBuild(state, operation.left);
      const right = requireBuild(state, operation.right);
      const relation =
        !left.inputsComplete ||
        !right.inputsComplete ||
        !left.outputsComplete ||
        !right.outputsComplete
          ? "incompleteEvidence"
          : left.recipeDigest !== right.recipeDigest ||
              left.lockDigest !== right.lockDigest ||
              left.toolchainDigest !== right.toolchainDigest ||
              left.targetDigest !== right.targetDigest ||
              left.runtimeClosureDigest !== right.runtimeClosureDigest ||
              left.environmentProjectionDigest !== right.environmentProjectionDigest
            ? "inputMismatch"
            : left.artifactDigest !== right.artifactDigest
              ? "artifactMismatch"
              : "reproducible";
      if (relation !== operation.relation) {
        throw new PackageReleaseError("unexpectedBuildRelation");
      }
      return;
    }
    case "createRelease": {
      if (state.releases[operation.release]) {
        throw new PackageReleaseError("releaseAlreadyCreated");
      }
      if (operation.builds.length === 0) {
        throw new PackageReleaseError("releaseRequiresBuildEvidence");
      }
      operation.builds.forEach((build) => requireBuild(state, build));
      const platformArtifactDigest = operation.platformArtifactBuild
        ? requireBuild(state, operation.platformArtifactBuild).artifactDigest
        : operation.platformArtifactDigest;
      if (typeof platformArtifactDigest !== "string" || platformArtifactDigest.length === 0) {
        throw new PackageReleaseError("releaseRequiresPlatformArtifact");
      }
      state.releases[operation.release] = {
        package: operation.package,
        version: operation.version,
        builds: operation.builds,
        requiredRebuilders: operation.requiredRebuilders,
        requiresPublicSource: operation.requiresPublicSource,
        requiresTransparency: operation.requiresTransparency,
        maintainerThresholdMet: operation.maintainerThresholdMet,
        sourcePublic: operation.sourcePublic,
        transparencyRecorded: operation.transparencyRecorded,
        maintainerIdentity: operation.maintainerIdentity,
        registryIdentity: operation.registryIdentity,
        toolchainProviderIdentity: operation.toolchainProviderIdentity,
        platformSignerIdentity: operation.platformSignerIdentity,
        platformArtifactDigest,
        sbomDigest: operation.sbomDigest,
        revoked: false,
        yanked: false,
        advisories: [],
        decision: null,
      };
      return;
    }
    case "verifyRelease": {
      const release = requireRelease(state, operation.release);
      const builds = release.builds.map((build) => requireBuild(state, build));
      if (release.revoked) {
        release.decision = "rejectRevoked";
        return;
      }
      if (release.yanked) {
        release.decision = "rejectYanked";
        return;
      }
      if (!release.maintainerThresholdMet) {
        release.decision = "rejectSignature";
        return;
      }
      if (roleCollision(release, builds)) {
        release.decision = "rejectRoleCollision";
        return;
      }
      if (release.requiresTransparency && !release.transparencyRecorded) {
        release.decision = "rejectTransparency";
        return;
      }
      if (
        builds.some((build) => !build.inputsComplete || !build.outputsComplete) ||
        builds.some(
          (build) =>
            build.recipeDigest !== builds[0].recipeDigest ||
            build.artifactDigest !== builds[0].artifactDigest,
        ) ||
        builds[0].artifactDigest !== release.platformArtifactDigest
      ) {
        release.decision = "rejectReproduction";
        return;
      }
      if (builds.length < release.requiredRebuilders) {
        release.decision = "published";
        return;
      }
      if (maximumIndependentCount(builds) < release.requiredRebuilders) {
        release.decision = "rejectReproduction";
        return;
      }
      if (release.requiresPublicSource && !release.sourcePublic) {
        release.decision = "rejectSourceAccess";
        return;
      }
      release.decision = release.sourcePublic ? "reproducible" : "privatelyReproducible";
      return;
    }
    case "addAdvisory": {
      const release = requireRelease(state, operation.release);
      if (!release.advisories.includes(operation.advisory)) {
        release.advisories.push(operation.advisory);
      }
      return;
    }
    case "yankRelease": {
      requireRelease(state, operation.release).yanked = true;
      return;
    }
    case "revokeRelease": {
      requireRelease(state, operation.release).revoked = true;
      return;
    }
    case "verifyReleaseState": {
      const release = requireRelease(state, operation.release);
      if (operation.decision !== undefined && release.decision !== operation.decision) {
        throw new PackageReleaseError("unexpectedReleaseDecision");
      }
      if (
        operation.advisories !== undefined &&
        JSON.stringify(release.advisories) !== JSON.stringify(operation.advisories)
      ) {
        throw new PackageReleaseError("unexpectedAdvisoryState");
      }
      return;
    }
    default:
      throw new PackageReleaseError("unknownPackageReleaseOperation");
  }
}

export function validatePackageReleaseOperation(operation) {
  if (!operation || typeof operation !== "object" || typeof operation.op !== "string") {
    return false;
  }
  const string = (field) => typeof operation[field] === "string" && operation[field].length > 0;
  switch (operation.op) {
    case "acceptRegistrySnapshot":
      return (
        string("snapshot") &&
        Number.isInteger(operation.version) &&
        operation.version > 0 &&
        typeof operation.thresholdValid === "boolean" &&
        typeof operation.delegationValid === "boolean" &&
        typeof operation.fresh === "boolean" &&
        Array.isArray(operation.candidates) &&
        operation.candidates.every(
          (candidate) =>
            candidate &&
            typeof candidate.identity === "string" &&
            candidate.identity.length > 0 &&
            typeof candidate.version === "string" &&
            typeof candidate.source === "string" &&
            typeof candidate.content === "string" &&
            (candidate.dependencies === undefined ||
              (Array.isArray(candidate.dependencies) &&
                candidate.dependencies.every(
                  (dependency) =>
                    dependency &&
                    typeof dependency.identity === "string" &&
                    dependency.identity.length > 0 &&
                    typeof dependency.constraint === "string",
                ))),
        )
      );
    case "createRealm":
      return string("realm") && string("snapshot");
    case "addRequirement":
      return (
        string("realm") && string("identity") && string("constraint") && string("from")
      );
    case "solveRealm":
      return string("realm");
    case "verifySelection":
      return string("realm") && string("identity");
    case "writeLock":
      return (
        string("lock") &&
        string("resolver") &&
        string("workspaceDigest") &&
        operation.manifestDigests &&
        typeof operation.manifestDigests === "object" &&
        operation.sourceInventories &&
        typeof operation.sourceInventories === "object" &&
        Object.values(operation.sourceInventories).every(Array.isArray) &&
        Array.isArray(operation.contexts) &&
        operation.contexts.length > 0 &&
        operation.contexts.every(
          (context) =>
            context &&
            ["realm", "root", "target", "use", "activeSourceSet"].every(
              (field) => typeof context[field] === "string" && context[field].length > 0,
            ),
        )
      );
    case "validateLocked":
    case "verifyLock":
      return string("lock");
    case "declareObject":
    case "storeObject":
      return (
        string("object") &&
        typeof operation.content === "string" &&
        (operation.kind === undefined || string("kind"))
      );
    case "registerMirror":
      return string("mirror") && typeof operation.listed === "boolean";
    case "fetchMirror":
      return (
        string("mirror") &&
        string("object") &&
        typeof operation.content === "string" &&
        Number.isInteger(operation.metadataVersion)
      );
    case "requireOffline":
      return Array.isArray(operation.objects);
    case "createRecipe":
      return (
        string("recipe") &&
        string("lock") &&
        string("product") &&
        string("target") &&
        string("profile") &&
        string("toolchainPlanDigest") &&
        string("runtimeClosureDigest") &&
        Array.isArray(operation.sourceTrees) &&
        operation.sourceTrees.every(
          (source) =>
            source &&
            typeof source.package === "string" &&
            source.package.length > 0 &&
            typeof source.object === "string" &&
            source.object.length > 0,
        ) &&
        Array.isArray(operation.inputs) &&
        operation.inputs.every((input) => typeof input === "string" && input.length > 0)
      );
    case "verifyRecipeRelation":
      return string("left") && string("right") && typeof operation.equal === "boolean";
    case "runBuild":
      return (
        string("build") &&
        string("recipe") &&
        typeof operation.payload === "string" &&
        string("builderIdentity") &&
        string("operatorIdentity") &&
        string("credentialIdentity") &&
        string("executionRootIdentity") &&
        string("executorIdentity")
      );
    case "verifyBuildRelation":
      return string("left") && string("right") && string("relation");
    case "createRelease":
      return (
        string("release") &&
        string("package") &&
        string("version") &&
        Array.isArray(operation.builds) &&
        operation.builds.length > 0 &&
        operation.builds.every((build) => typeof build === "string" && build.length > 0) &&
        Number.isInteger(operation.requiredRebuilders) &&
        operation.requiredRebuilders > 0 &&
        typeof operation.requiresPublicSource === "boolean" &&
        typeof operation.requiresTransparency === "boolean" &&
        typeof operation.maintainerThresholdMet === "boolean" &&
        typeof operation.sourcePublic === "boolean" &&
        typeof operation.transparencyRecorded === "boolean" &&
        string("maintainerIdentity") &&
        string("registryIdentity") &&
        string("toolchainProviderIdentity") &&
        string("platformSignerIdentity") &&
        string("sbomDigest") &&
        (string("platformArtifactBuild") || string("platformArtifactDigest"))
      );
    case "verifyRelease":
    case "yankRelease":
    case "revokeRelease":
    case "verifyReleaseState":
      return string("release");
    case "addAdvisory":
      return string("release") && string("advisory");
    default:
      return false;
  }
}

export function runPackageReleaseProgram(operations) {
  const state = {
    trustedSnapshotVersion: 0,
    snapshots: {},
    realms: {},
    locks: {},
    objects: {},
    cas: {},
    mirrors: {},
    recipes: {},
    builds: {},
    recipeOutputs: {},
    conflictingRecipeOutputs: {},
    releases: {},
  };
  const trace = [];
  for (const [index, operation] of operations.entries()) {
    try {
      applyOperation(state, operation);
      trace.push({ index, op: operation.op, accepted: true });
    } catch (error) {
      if (!(error instanceof PackageReleaseError)) throw error;
      trace.push({ index, op: operation.op, rejected: error.code });
      return { status: "rejected", code: error.code, operation: index, state, trace };
    }
  }
  return { status: "accepted", state, trace };
}
