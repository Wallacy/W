// Typed, bounded presentation contracts for PYN3.
//
// This file is a parseable SDK draft.  The provider is missing.  Writer is an
// opaque sink.  It does not expose a public constructor, object map, Any value,
// active media pass-through, or an ambient display singleton.

import json from std.json

export enum MediaKind: Copy & Equatable {
  plainText
  json
  png
  jpeg
  vendorJson
  html
  svg
}

export enum LimitKind: Copy & Equatable {
  totalBytes
  representationCount
  textBytes
  imageBytes
  imageWidth
  imageHeight
  imagePixels
  jsonDepth
  jsonNodes
  jsonStringBytes
  workUnits
}

export enum MediaTypeError: Error {
  invalidSyntax
  unsupported
  activeContent
  providerMissing
}

export enum Error: Error {
  invalidMedia(MediaTypeError)
  duplicateMedia(String)
  missingPlainText
  limitExceeded(LimitKind, usize)
  jsonEncode(json.EncodeError)
  writerClosed
}

export struct Limits: Copy & Equatable {
  let storedMaximumBytes: usize<(1...)>
  let storedMaximumRepresentations: usize<(1...)>
  let storedMaximumTextBytes: usize<(1...)>
  let storedMaximumImageBytes: usize<(1...)>
  let storedMaximumImageWidth: u32<(1...)>
  let storedMaximumImageHeight: u32<(1...)>
  let storedMaximumImagePixels: u64<(1...)>
  let storedMaximumJsonDepth: usize<(1...)>
  let storedMaximumJsonNodes: usize<(1...)>
  let storedMaximumJsonStringBytes: usize<(1...)>
  let storedMaximumWorkUnits: usize<(1...)>

  export let maximumBytes: usize<(1...)> { get => storedMaximumBytes }
  export let maximumRepresentations: usize<(1...)> {
    get => storedMaximumRepresentations
  }
  export let maximumTextBytes: usize<(1...)> { get => storedMaximumTextBytes }
  export let maximumImageBytes: usize<(1...)> { get => storedMaximumImageBytes }
  export let maximumImageWidth: u32<(1...)> { get => storedMaximumImageWidth }
  export let maximumImageHeight: u32<(1...)> { get => storedMaximumImageHeight }
  export let maximumImagePixels: u64<(1...)> { get => storedMaximumImagePixels }
  export let maximumJsonDepth: usize<(1...)> { get => storedMaximumJsonDepth }
  export let maximumJsonNodes: usize<(1...)> { get => storedMaximumJsonNodes }
  export let maximumJsonStringBytes: usize<(1...)> { get => storedMaximumJsonStringBytes }
  export let maximumWorkUnits: usize<(1...)> { get => storedMaximumWorkUnits }

  export const init(
    maximumBytes: usize<(1...)>,
    maximumRepresentations: usize<(1...)>,
    maximumTextBytes: usize<(1...)>,
    maximumImageBytes: usize<(1...)>,
    maximumImageWidth: u32<(1...)>,
    maximumImageHeight: u32<(1...)>,
    maximumImagePixels: u64<(1...)>,
    maximumJsonDepth: usize<(1...)>,
    maximumJsonNodes: usize<(1...)>,
    maximumJsonStringBytes: usize<(1...)>,
    maximumWorkUnits: usize<(1...)>,
  ) {
    self.storedMaximumBytes = maximumBytes
    self.storedMaximumRepresentations = maximumRepresentations
    self.storedMaximumTextBytes = maximumTextBytes
    self.storedMaximumImageBytes = maximumImageBytes
    self.storedMaximumImageWidth = maximumImageWidth
    self.storedMaximumImageHeight = maximumImageHeight
    self.storedMaximumImagePixels = maximumImagePixels
    self.storedMaximumJsonDepth = maximumJsonDepth
    self.storedMaximumJsonNodes = maximumJsonNodes
    self.storedMaximumJsonStringBytes = maximumJsonStringBytes
    self.storedMaximumWorkUnits = maximumWorkUnits
  }

  export const init(maximumBytes: usize<(1...)>) {
    self.storedMaximumBytes = maximumBytes
    self.storedMaximumRepresentations = 16
    self.storedMaximumTextBytes = maximumBytes
    self.storedMaximumImageBytes = maximumBytes
    self.storedMaximumImageWidth = 4096
    self.storedMaximumImageHeight = 4096
    self.storedMaximumImagePixels = 16_777_216_u64
    self.storedMaximumJsonDepth = 64
    self.storedMaximumJsonNodes = maximumBytes
    self.storedMaximumJsonStringBytes = maximumBytes
    self.storedMaximumWorkUnits = maximumBytes
  }
}

// MediaType accepts only the validated baseline set.  HTML and SVG are
// represented so a provider can report missing sanitizer policy explicitly.
export struct MediaType: Equatable {
  let storedValue: String

  export init(value: String) throws MediaTypeError {
    self.storedValue = unsafe {
      try stdPresentationMediaTypeValidate(ref value)
    }
  }

  init(validatedValue: String) {
    self.storedValue = take validatedValue
  }

  export let value: view String { get => storedValue }

  export fn kind(): MediaKind throws MediaTypeError {
    return unsafe { try stdPresentationMediaTypeKind(ref self) }
  }

  export fn isPortable(): Bool {
    return unsafe { stdPresentationMediaTypePortable(ref self) }
  }
}

// Images are typed at the API boundary.  A caller cannot attach an arbitrary
// media string to image bytes; the provider validates the encoding and bounds.
export struct Png {
  let bytes: Bytes
  let width: u32
  let height: u32

  export init(bytes: take Bytes, width: u32, height: u32) throws Error {
    self.bytes = unsafe { try stdPresentationPngValidate(take bytes, width, height) }
    self.width = width
    self.height = height
  }
}

export struct Jpeg {
  let bytes: Bytes
  let width: u32
  let height: u32

  export init(bytes: take Bytes, width: u32, height: u32) throws Error {
    self.bytes = unsafe { try stdPresentationJpegValidate(take bytes, width, height) }
    self.width = width
    self.height = height
  }
}

export protocol Presentable {
  fn present(to writer: inout Writer) throws Error
}

// Writer is created by the host with Limits for one presentation call.  The
// raw handle and its lifetime never cross the call or become a public object.
export struct Writer {
  let handle: PresentationWriterHandle

  init(validatedHandle: PresentationWriterHandle) {
    self.handle = validatedHandle
  }

  export mut fn text(value: view String) throws Error {
    unsafe {
      try stdPresentationWriterText(inout handle, value)
    }
  }

  export mut fn png(image: ref Png) throws Error {
    unsafe {
      try stdPresentationWriterPng(
        inout handle,
        image,
      )
    }
  }

  export mut fn jpeg(image: ref Jpeg) throws Error {
    unsafe {
      try stdPresentationWriterJpeg(inout handle, image)
    }
  }

  export mut fn json(
    body: some take fn(inout json.Writer): () throws json.EncodeError,
  ) throws Error {
    unsafe {
      try stdPresentationWriterJson(inout handle, take body)
    }
  }

  export mut fn vendorJson(
    media: ref MediaType,
    body: some take fn(inout json.Writer): () throws json.EncodeError,
  ) throws Error {
    unsafe {
      try stdPresentationWriterVendorJson(
        inout handle,
        media,
        take body,
      )
    }
  }

  deinit {
    unsafe { stdPresentationWriterDrop(inout handle) }
  }
}

foreign intrinsic from "std.presentation@1" {
  type PresentationWriterHandle

  fn stdPresentationMediaTypeValidate(
    _ value: ref String,
  ): String throws MediaTypeError
  fn stdPresentationMediaTypeKind(
    _ media: ref MediaType,
  ): MediaKind throws MediaTypeError
  fn stdPresentationMediaTypePortable(_ media: ref MediaType): Bool

  fn stdPresentationPngValidate(
    _ bytes: take Bytes,
    _ width: u32,
    _ height: u32,
  ): Bytes throws Error
  fn stdPresentationJpegValidate(
    _ bytes: take Bytes,
    _ width: u32,
    _ height: u32,
  ): Bytes throws Error

  fn stdPresentationWriterText(
    _ handle: inout PresentationWriterHandle,
    _ value: view String,
  ) throws Error
  fn stdPresentationWriterPng(
    _ handle: inout PresentationWriterHandle,
    _ image: ref Png,
  ) throws Error
  fn stdPresentationWriterJpeg(
    _ handle: inout PresentationWriterHandle,
    _ image: ref Jpeg,
  ) throws Error
  fn stdPresentationWriterJson(
    _ handle: inout PresentationWriterHandle,
    _ body: take some take fn(inout json.Writer): () throws json.EncodeError,
  ) throws Error
  fn stdPresentationWriterVendorJson(
    _ handle: inout PresentationWriterHandle,
    _ media: ref MediaType,
    _ body: take some take fn(inout json.Writer): () throws json.EncodeError,
  ) throws Error
  fn stdPresentationWriterDrop(_ handle: inout PresentationWriterHandle)
}
