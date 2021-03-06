#include "common.h"

memcached_return memcached_string_check(memcached_string_st *string, size_t need)
{
  if (need && need > (size_t)(string->current_size - (size_t)(string->end - string->string)))
  {
    size_t current_offset= string->end - string->string;
    char *new_value;
    size_t adjust;
    size_t new_size;

    /* This is the block multiplier. To keep it larger and surive division errors we must round it up */
    adjust= (need - (size_t)(string->current_size - (size_t)(string->end - string->string))) / string->block_size;
    adjust++;

    new_size= sizeof(char) * (size_t)((adjust * string->block_size) + string->current_size);
    /* Test for overflow */
    if (new_size < need)
      return MEMCACHED_MEMORY_ALLOCATION_FAILURE;

    if (string->root->call_realloc)
      new_value= (char *)string->root->call_realloc(string->root, string->string, new_size);
    else
      new_value= (char *)realloc(string->string, new_size);

    if (new_value == NULL)
      return MEMCACHED_MEMORY_ALLOCATION_FAILURE;

    string->string= new_value;
    string->end= string->string + current_offset;

    string->current_size+= (string->block_size * adjust);
  }

  return MEMCACHED_SUCCESS;
}

memcached_string_st *memcached_string_create(memcached_st *ptr, memcached_string_st *string, size_t initial_size)
{
  memcached_return rc;

  /* Saving malloc calls :) */
  if (string)
  {
    memset(string, 0, sizeof(memcached_string_st));
    string->is_allocated= MEMCACHED_NOT_ALLOCATED;
  }
  else
  {
    if (ptr->call_malloc)
      string= (memcached_string_st *)ptr->call_malloc(ptr, sizeof(memcached_string_st));
    else
      string= (memcached_string_st *)malloc(sizeof(memcached_string_st));

    if (string == NULL)
      return NULL;
    memset(string, 0, sizeof(memcached_string_st));
    string->is_allocated= MEMCACHED_ALLOCATED;
  }
  string->block_size= MEMCACHED_BLOCK_SIZE;
  string->root= ptr;

  rc=  memcached_string_check(string, initial_size);
  if (rc != MEMCACHED_SUCCESS)
  {
    if (ptr->call_free)
      ptr->call_free(ptr, string);
    else
      free(string);

    return NULL;
  }

  WATCHPOINT_ASSERT(string->string == string->end);

  return string;
}

memcached_return memcached_string_append_character(memcached_string_st *string, 
                                                   char character)
{
  memcached_return rc;

  WATCHPOINT_ASSERT(string->is_allocated != MEMCACHED_USED);

  rc=  memcached_string_check(string, 1);

  if (rc != MEMCACHED_SUCCESS)
    return rc;

  *string->end= ' ';
  string->end++;

  return MEMCACHED_SUCCESS;
}

memcached_return memcached_string_append(memcached_string_st *string,
                                         char *value, size_t length)
{
  memcached_return rc;

  WATCHPOINT_ASSERT(string->is_allocated != MEMCACHED_USED);

  rc= memcached_string_check(string, length);

  if (rc != MEMCACHED_SUCCESS)
    return rc;

  WATCHPOINT_ASSERT(length <= string->current_size);
  WATCHPOINT_ASSERT(string->string);
  WATCHPOINT_ASSERT(string->end >= string->string);

  memcpy(string->end, value, length);
  string->end+= length;

  return MEMCACHED_SUCCESS;
}

size_t memcached_string_backspace(memcached_string_st *string, size_t remove)
{
  WATCHPOINT_ASSERT(string->is_allocated != MEMCACHED_USED);

  if (string->end - string->string  > remove)
  {
    size_t difference;

    difference= string->end - string->string;
    string->end= string->string;

    return difference;
  }
  string->end-= remove;

  return remove;
}

char *memcached_string_c_copy(memcached_string_st *string)
{
  char *c_ptr;

  WATCHPOINT_ASSERT(string->is_allocated != MEMCACHED_USED);

  if (memcached_string_length(string) == 0)
    return NULL;

  if (string->root->call_malloc)
    c_ptr= (char *)string->root->call_malloc(string->root, (memcached_string_length(string)+1) * sizeof(char));
  else
    c_ptr= (char *)malloc((memcached_string_length(string)+1) * sizeof(char));

  if (c_ptr == NULL)
    return NULL;

  memcpy(c_ptr, memcached_string_value(string), memcached_string_length(string));
  c_ptr[memcached_string_length(string)]= 0;

  return c_ptr;
}

memcached_return memcached_string_reset(memcached_string_st *string)
{
  WATCHPOINT_ASSERT(string->is_allocated != MEMCACHED_USED);
  string->end= string->string;
  
  return MEMCACHED_SUCCESS;
}

void memcached_string_free(memcached_string_st *ptr)
{
  if (ptr == NULL)
    return;

  if (ptr->string)
  {
    if (ptr->root->call_free)
      ptr->root->call_free(ptr->root, ptr->string);
    else
      free(ptr->string);
  }

  if (ptr->is_allocated == MEMCACHED_ALLOCATED)
  {
    if (ptr->root->call_free)
      ptr->root->call_free(ptr->root, ptr);
    else
      free(ptr);
  }
  else
    ptr->is_allocated= MEMCACHED_USED;
}
