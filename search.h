#ifndef SEARCH_H
#define SEARCH_H

#include <windows.h>
#include <string>

#ifdef __cplusplus
extern "C" {
#endif

bool search_initialize();
void search_cleanup();

void search_set_scope(int scope, const char* customPath);
void search_start_indexing();
bool search_is_indexing();
int search_get_progress();
void search_cancel_indexing();

void search_set_keywords(const char* keywords);
void search_execute();
int search_get_result_count();

void search_run_ui(void* appState);
bool search_is_active();
void search_close();

#ifdef __cplusplus
}
#endif

#endif
