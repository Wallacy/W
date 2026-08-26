#ifndef W_SEED_MODULE_SCAN_H
#define W_SEED_MODULE_SCAN_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "w_seed_parser.h"
#include "w_seed_source.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The module scanner is an internal, caller-owned seed API. It does not
 * allocate, retain source storage, or use mutable global state. */
typedef enum {
  W_SEED_MODULE_SCAN_OK = 0,
  W_SEED_MODULE_SCAN_CAPACITY,
  W_SEED_MODULE_SCAN_INVALID,
  W_SEED_MODULE_SCAN_UNSUPPORTED,
} w_seed_module_scan_status;

typedef enum {
  W_SEED_MODULE_ORIGIN_IMPORT = 0,
} w_seed_module_origin_kind;

typedef struct {
  w_seed_module_origin_kind kind;
  uint32_t direct_import_ordinal;
  w_seed_cst_index cst_node_index;
  w_seed_span declaration_span;
  w_seed_span module_path_span;
} w_seed_module_origin;

typedef struct {
  w_seed_module_scan_status status;
  size_t required;
  size_t written;
  bool has_module_header_name;
  w_seed_span module_header_name_span;
} w_seed_module_scan_result;

/* Scan one complete CST document. The source, nodes, and parse result remain
 * caller-owned. Pass origins=NULL and origin_capacity=0 to measure. */
w_seed_module_scan_status w_seed_module_scan(
    const w_seed_source *source, const w_seed_cst_node *nodes,
    size_t node_count, const w_seed_parse_result *parse,
    w_seed_module_origin *origins, size_t origin_capacity,
    w_seed_module_scan_result *result);

/* Return the exact source span of one parser-valid import path. The helper is
 * shared by the scanner and frontend. It returns false for malformed or
 * unsupported import forms. */
bool w_seed_module_scan_import_path_span(
    const w_seed_source *source, const w_seed_cst_node *nodes,
    size_t node_count, w_seed_span declaration_span, w_seed_span *path_span);

#ifdef __cplusplus
}
#endif

#endif
