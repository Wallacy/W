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
  storedMaximumBytes: usize<(1...)>
  storedMaximumRepresentations: usize<(1...)>
  storedMaximumTextBytes: usize<(1...)>
  storedMaximumImageBytes: usize<(1...)>
  storedMaximumImageWidth: u32<(1...)>
  storedMaximumImageHeight: u32<(1...)>
  storedMaximumImagePixels: u64<(1...)>
  storedMaximumJsonDepth: usize<(1...)>
  storedMaximumJsonNodes: usize<(1...)>
  storedMaximumJsonStringBytes: usize<(1...)>
  storedMaximumWorkUnits: usize<(1...)>

  export maximumBytes: usize<(1...)> { get => storedMaximumBytes }
  export maximumRepresentations: usize<(1...)> {
    get => storedMaximumRepresentations
  }
  export maximumTextBytes: usize<(1...)> { get => storedMaximumTextBytes }
  export maximumImageBytes: usize<(1...)> { get => storedMaximumImageBytes }
  export maximumImageWidth: u32<(1...)> { get => storedMaximumImageWidth }
  export maximumImageHeight: u32<(1...)> { get => storedMaximumImageHeight }
  export maximumImagePixels: u64<(1...)> { get => storedMaximumImagePixels }
  export maximumJsonDepth: usize<(1...)> { get => storedMaximumJsonDepth }
  export maximumJsonNodes: usize<(1...)> { get => storedMaximumJsonNodes }
  export maximumJsonStringBytes: usize<(1...)> { get => storedMaximumJsonStringBytes }
  export maximumWorkUnits: usize<(1...)> { get => storedMaximumWorkUnits }

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
  storedValue: String

  export init(_ value: String) throws MediaTypeError {
    self.storedValue = unsafe {
      try stdPresentationMediaTypeValidate(value: ref value)
    }
  }

  init(validatedValue: String) {
    self.storedValue = take validatedValue
  }

  export value: view String { get => storedValue }

  export fn kind(): MediaKind throws MediaTypeError {
    return unsafe { try stdPresentationMediaTypeKind(media: ref self) }
  }

  export fn isPortable(): Bool {
    return unsafe { stdPresentationMediaTypePortable(media: ref self) }
  }
}

// Images are typed at the API boundary.  A caller cannot attach an arbitrary
// media string to image bytes; the provider validates the encoding and bounds.
export struct Png {
  bytes: Bytes
  width: u32
  height: u32

  export init(bytes: take Bytes, width: u32, height: u32) throws Error {
    self.bytes = unsafe { try stdPresentationPngValidate(bytes: take bytes, width: width, height: height) }
    self.width = width
    self.height = height
  }
}

export struct Jpeg {
  bytes: Bytes
  width: u32
  height: u32

  export init(bytes: take Bytes, width: u32, height: u32) throws Error {
    self.bytes = unsafe { try stdPresentationJpegValidate(bytes: take bytes, width: width, height: height) }
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
  handle: PresentationWriterHandle

  init(validatedHandle: PresentationWriterHandle) {
    self.handle = validatedHandle
  }

  export mut fn text(_ value: view String) throws Error {
    unsafe {
      try stdPresentationWriterText(handle: inout handle, value: value)
    }
  }

  export mut fn png(_ image: ref Png) throws Error {
    unsafe {
      try stdPresentationWriterPng(
        handle: inout handle,
        image: image,
      )
    }
  }

  export mut fn jpeg(_ image: ref Jpeg) throws Error {
    unsafe {
      try stdPresentationWriterJpeg(handle: inout handle, image: image)
    }
  }

  export mut fn json(
    _ body: some take fn(inout json.Writer): () throws json.EncodeError,
  ) throws Error {
    unsafe {
      try stdPresentationWriterJson(handle: inout handle, body: take body)
    }
  }

  export mut fn vendorJson(
    media: ref MediaType,
    _ body: some take fn(inout json.Writer): () throws json.EncodeError,
  ) throws Error {
    unsafe {
      try stdPresentationWriterVendorJson(
        handle: inout handle,
        media: media,
        body: take body,
      )
    }
  }

  deinit {
    unsafe { stdPresentationWriterDrop(handle: inout handle) }
  }
}

foreign intrinsic from "std.presentation@1" {
  type PresentationWriterHandle

  fn stdPresentationMediaTypeValidate(
    value: ref String,
  ): String throws MediaTypeError
  fn stdPresentationMediaTypeKind(
    media: ref MediaType,
  ): MediaKind throws MediaTypeError
  fn stdPresentationMediaTypePortable(media: ref MediaType): Bool

  fn stdPresentationPngValidate(
    bytes: take Bytes,
    width: u32,
    height: u32,
  ): Bytes throws Error
  fn stdPresentationJpegValidate(
    bytes: take Bytes,
    width: u32,
    height: u32,
  ): Bytes throws Error

  fn stdPresentationWriterText(
    handle: inout PresentationWriterHandle,
    value: view String,
  ) throws Error
  fn stdPresentationWriterPng(
    handle: inout PresentationWriterHandle,
    image: ref Png,
  ) throws Error
  fn stdPresentationWriterJpeg(
    handle: inout PresentationWriterHandle,
    image: ref Jpeg,
  ) throws Error
  fn stdPresentationWriterJson(
    handle: inout PresentationWriterHandle,
    body: some take fn(inout json.Writer): () throws json.EncodeError,
  ) throws Error
  fn stdPresentationWriterVendorJson(
    handle: inout PresentationWriterHandle,
    media: ref MediaType,
    body: some take fn(inout json.Writer): () throws json.EncodeError,
  ) throws Error
  fn stdPresentationWriterDrop(handle: inout PresentationWriterHandle)
}
