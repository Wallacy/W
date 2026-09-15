module atlas_short_entry

// This file exists because a module can own only one `.default` descriptor.
// Keeping the short entry separate lets every accepted entry spelling remain
// a semantically possible module instead of stacking mutually exclusive roots.

// atlas:begin entry-short-body
entry {
  print("atlas short entry ready")
}
// atlas:end entry-short-body
