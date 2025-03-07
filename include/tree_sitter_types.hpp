#pragma once

// Forward declarations for tree-sitter types
// This avoids direct inclusion of tree_sitter/api.h in headers

#ifdef __cplusplus
extern "C" {
#endif

// We need to ensure we use the same definition for TSNode as tree-sitter
// This matches the definition in tree_sitter/api.h
typedef struct {
    const void* tree;
    const void* id;
    uint32_t context[4];
} TSNode;

// Forward declare tree-sitter types used in our codebase
typedef struct TSLanguage TSLanguage;
typedef struct TSParser TSParser;
typedef struct TSTree TSTree;
typedef struct TSQuery TSQuery;
typedef struct TSQueryCursor TSQueryCursor;

// Structure definitions needed for allocation
typedef struct {
    TSNode node;
    uint32_t index;
} TSQueryCapture;

typedef struct {
    uint32_t id;
    uint16_t pattern_index;
    uint16_t capture_count;
    const TSQueryCapture *captures;
} TSQueryMatch;

// Enum definitions
typedef enum {
    TSQueryErrorNone = 0,
    TSQueryErrorSyntax,
    TSQueryErrorNodeType,
    TSQueryErrorField,
    TSQueryErrorCapture,
    TSQueryErrorStructure,
    TSQueryErrorLanguage,
} TSQueryError;

// Forward declare tree-sitter language functions
TSLanguage* tree_sitter_cpp();
TSLanguage* tree_sitter_c();
TSLanguage* tree_sitter_python();
TSLanguage* tree_sitter_javascript();

// Forward declare tree-sitter parser functions
TSParser* ts_parser_new();
void ts_parser_delete(TSParser* parser);
bool ts_parser_set_language(TSParser* parser, const TSLanguage* language);
TSTree* ts_parser_parse_string(
    TSParser* parser,
    const TSTree* old_tree,
    const char* string,
    uint32_t length
);

// Forward declare tree-sitter tree functions
TSNode ts_tree_root_node(const TSTree* tree);
void ts_tree_delete(TSTree* tree);

// Forward declare tree-sitter node functions
uint32_t ts_node_start_byte(TSNode node);
uint32_t ts_node_end_byte(TSNode node);

// Forward declare tree-sitter query functions
TSQuery* ts_query_new(
    const TSLanguage* language,
    const char* source,
    uint32_t source_len,
    uint32_t* error_offset,
    TSQueryError* error_type
);
void ts_query_delete(TSQuery* query);
void ts_query_capture_name_for_id(
    const TSQuery* query,
    uint32_t index,
    const char** name,
    uint32_t* length
);

// Forward declare tree-sitter query cursor functions
TSQueryCursor* ts_query_cursor_new();
void ts_query_cursor_delete(TSQueryCursor* cursor);
void ts_query_cursor_exec(TSQueryCursor* cursor, const TSQuery* query, TSNode node);
bool ts_query_cursor_next_match(TSQueryCursor* cursor, TSQueryMatch* match);

#ifdef __cplusplus
}
#endif 