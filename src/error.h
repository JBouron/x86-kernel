#include <types.h>
#include <list.h>
#include <percpu.h>

// This file defines the error log/reporting mechanism. Each cpu has a private
// linked list of struct error_desc. Each struct error_desc contains the
// information about an error occuring in some scope, the linked list makes the
// full "stack" trace of the errors.

// Error description. This struct contain the information of where an error
// occured and why.
struct error_desc {
    // If true, this struct has been statically allocated.
    bool is_static;
    // If true, this description is active. This is used in statically allocated
    // error_desc.
    bool active;
    // The name of the file in which the error was set / occured.
    char const * file;
    // The line in the file at which the error was set / occured.
    uint32_t line;
    // The function in which the error was set / occured.
    char const * func;
    // Message describing the error.
    char const * message;
    // Error code corresponding to the error (if applicable).
    int32_t error_code;
    // Node of the linked list of error describing the whole chain of errors.
    struct list_node error_linked_list;
};

// Indicate per cpu the nest level of kmalloc calls. This is needed to indicate
// if a cpu can call kmalloc() when doing an allocation in SET_ERROR().
// The reason that we use a nest level and not a bool "in kmalloc" here is to
// avoid race conditions when kmalloc() temporarily releases the lock when
// creating a new group.
DECLARE_PER_CPU(uint32_t, kmalloc_nest_level) = 0;

// Add an struct error_desc to the current cpu's error linked list.
// @param file: The file where the error occured.
// @param line: The line number in the file where the error occured.
// @param func: The function name where the error occured.
// @param message: Message describing the error.
// @param error_code: Error code corresponding to the error.
// Note: This function is not meant to be used directly. Use the SET_ERROR()
// macro instead.
void _set_error(char const * const file,
                uint32_t const line,
                char const * const func,
                char const * message,
                int32_t const code);

// Shortcut to _set_error() where the file, line and func argument are
// automatically derived.
// @param message: Message describing the error.
// @param error_code: Error code corresponding to the error.
#define SET_ERROR(message, error_code) \
    _set_error(__FILE__, __LINE__, __func__, message, error_code)

// Clear the error list on the current cpu.
// Note: This function is not meant to be used directly. Use the CLEAR_ERROR()
// macro instead.
void _clear_error(void);

// Clear the error list on the current cpu.
#define CLEAR_ERROR() \
    _clear_error()

// Execute tests related to error reporting.
void error_test(void);
