// Bounded Web body carriers for horizon observations.

import { Blob } from std.blob
import http from std.http

export enum HorizonBodyError: Error {
  formData(http.FormDataError)
  request(http.RequestError)
  response(http.ResponseError)
}

export fn horizonObservationForm(
  named station: take String,
  named image: take Bytes,
): http.FormData throws http.FormDataError {
  var form = http.FormData(
    limits: http.FormDataLimits(
      maximumEntries: 8,
      maximumNameBytes: 128,
      maximumFilenameBytes: 256,
      maximumTextBytes: 4<KiB>,
      maximumBlobBytes: 8<MiB>,
      maximumPayloadBytes: 8<MiB>,
      maximumEncodedBytes: 9<MiB>,
    ),
  )

  try form.append(name: "station", value: take station)
  try form.append(name: "sensor", value: "Violet Horizon")
  try form.append(name: "sensor", value: "Patient Comet")

  let snapshot = Blob(take image, type: "IMAGE/JPEG")
  try form.append(
    name: "horizon",
    blob: take snapshot,
    filename: "violet-horizon.jpg",
  )
  return form
}

// Provider-gated: construction validates FormData and MessageLimits before it
// publishes the Request.  The HTTP adapter, not this source, chooses boundary.
export fn horizonObservationRequest(
  endpoint: String,
  station: String,
  image: take Bytes,
): http.Request throws HorizonBodyError {
  var form: http.FormData
  do {
    form = try horizonObservationForm(
      station: take station,
      image: take image,
    )
  } catch error {
    throw .formData(error)
  }

  do {
    return try http.Request(
      take endpoint,
      init: http.RequestInit(
        method: .some(.post),
        body: .some(.formData(take form)),
      ),
    )
  } catch error {
    throw .request(error)
  }
}

// Provider-gated: the same logical carrier can be the complete response body.
export fn horizonSnapshotResponse(
  image: take Bytes,
): http.Response throws HorizonBodyError {
  let snapshot = Blob(take image, type: "image/jpeg")
  do {
    return try http.Response(take snapshot)
  } catch error {
    throw .response(error)
  }
}

export take async fn readHorizonObservation(
  request: http.Request,
): http.FormData throws http.BodyDecodeError<http.FormDataError> {
  return try await (take request).formData(
    limits: http.FormDataLimits.standard(),
  )
}

test "FormData preserves repeated sensor order and Blob metadata" for horizonObservationForm {
  let form = try horizonObservationForm(
    station: "Milliways",
    image: take b"\xff\xd8\xff\xd9",
  )
  let sensors = form.getAll("sensor")
  let entries = form.entries()

  expect form.size == 4
  expect sensors.count == 2
  expect entries[0].name() == "station"
  expect entries[3].name() == "horizon"

  switch entries[3].value() {
    case .blob(let image, let filename):
      expect image.type == "image/jpeg"
      expect image.size == 4
      expect filename == "violet-horizon.jpg"
    case .text(_):
      panic("the horizon image became a text field")
  }
}

test "a failed FormData mutation preserves the previous list" {
  var form = http.FormData(
    limits: http.FormDataLimits(
      maximumEntries: 1,
      maximumNameBytes: 16,
      maximumFilenameBytes: 16,
      maximumTextBytes: 8,
      maximumBlobBytes: 8,
      maximumPayloadBytes: 8,
      maximumEncodedBytes: 256,
    ),
  )
  try form.append(name: "course", value: "End")

  do {
    try form.append(name: "guest", value: "Arthur")
    panic("an entry beyond the bound was accepted")
  } catch .limitExceeded(let kind, let maximum) {
    expect kind == .entries
    expect maximum == 1
  }

  expect form.size == 1
  expect form.entries()[0].name() == "course"
}
