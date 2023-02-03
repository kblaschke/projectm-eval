#pragma once

#include "CompilerTypes.h"

/**
 * @brief Creates an empty compile context.
 * @param global_memory An optional pointer to a memory buffer to use as global memory (gmegabuf). If NULL, uses the built-in buffer.
 * @return A pointer to the newly created context.
 */
prjm_eel_compiler_context_t* prjm_eel_create_compile_context(prjm_eel_mem_buffer global_memory);

/**
 * @brief Destroys a compile context.
 * Do not use the pointer afterwards.
 * @param cctx The context to destroy.
 */
void prjm_eel_destroy_compile_context(prjm_eel_compiler_context_t* cctx);

/**
 * @brief Compiles a program and returns a pointer to the result.
 * @param cctx The context to use for compilation.
 * @param code The code to compile.
 * @return A pointer to the resulting program tree or NULL on a parse error.
 */
prjm_eel_program_t* prjm_eel_compile_code(prjm_eel_compiler_context_t* cctx, const char* code);

/**
 * @brief Destroys a previously compiled program.
 * @param program The program to destroy.
 */
void prjm_eel_destroy_code(prjm_eel_program_t* program);

/**
 * @brief Resets all internal variable values to 0.
 * Externally registered variables are not changed.
 * @param cctx The compile context containing the variables.
 */
void prjm_eel_reset_context_vars(prjm_eel_compiler_context_t* cctx);

/**
 * @brief Returns the last error message.
 * @param cctx The context to retrieve the error from.
 * @param line A pointer to a variable that will receive the error's position line number or NULL.
 * @param column A pointer to a variable that will receive the error's position column number or NULL.
 * @return A char pointer to the last error message or NULL if there wasn't an error. The pointer is owned by the context, do not free.
 */
const char* prjm_eel_compiler_get_error(prjm_eel_compiler_context_t* cctx, int* line, int* column);