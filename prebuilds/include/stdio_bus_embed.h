/**
 * @file stdio_bus_embed.h
 * @brief Embedding API for stdio Bus kernel
 *
 * This header provides the embedding API for integrating stdio_bus into
 * host applications and language runtimes (Node.js N-API, Python C extension, etc.)
 *
 * Key design principles:
 * - No global state: all state encapsulated in stdio_bus_t
 * - Non-blocking: stdio_bus_step() returns immediately
 * - Callback-driven: host receives messages via callbacks
 * - Single-threaded: must be called from one thread (integrate with host event loop)
 *
 * Typical usage:
 * @code{.c}
 * stdio_bus_options_t opts = {
 *     .config_path = "config.json",
 *     .on_message = my_message_handler,
 *     .on_error = my_error_handler,
 *     .on_log = my_log_handler,
 *     .user_data = my_context
 * };
 *
 * stdio_bus_t *bus = stdio_bus_create(&opts);
 * stdio_bus_start(bus);
 *
 * // In host event loop (e.g., libuv uv_prepare)
 * while (running) {
 *     stdio_bus_step(bus, 0);  // non-blocking
 * }
 *
 * stdio_bus_stop(bus);
 * stdio_bus_destroy(bus);
 * @endcode
 */

#ifndef STDIO_BUS_EMBED_H
#define STDIO_BUS_EMBED_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Version Information
 *============================================================================*/

#define STDIO_BUS_EMBED_API_VERSION 2

/*============================================================================
 * Opaque Handle
 *============================================================================*/

/** Opaque stdio_bus instance handle */
typedef struct stdio_bus stdio_bus_t;

/*============================================================================
 * Callback Types
 *============================================================================*/

/**
 * @brief Message callback - called when a message is ready for the host
 *
 * This is called when:
 * - A response comes back from a worker (for requests sent via stdio_bus_ingest)
 * - A notification arrives from a worker
 *
 * @param bus      The stdio_bus instance
 * @param msg      Message data (JSON, null-terminated)
 * @param len      Message length (excluding null terminator)
 * @param user_data User context from options
 */
typedef void (*stdio_bus_message_cb)(stdio_bus_t *bus, const char *msg, 
                                     size_t len, void *user_data);

/**
 * @brief Error callback - called on errors
 *
 * @param bus       The stdio_bus instance
 * @param code      Error code (STDIO_BUS_ERR_*)
 * @param message   Human-readable error message
 * @param user_data User context from options
 */
typedef void (*stdio_bus_error_cb)(stdio_bus_t *bus, int code,
                                   const char *message, void *user_data);

/**
 * @brief Log callback - called for log messages
 *
 * If not provided, logs go to stderr.
 *
 * @param bus       The stdio_bus instance
 * @param level     Log level (0=DEBUG, 1=INFO, 2=WARN, 3=ERROR)
 * @param message   Log message
 * @param user_data User context from options
 */
typedef void (*stdio_bus_log_cb)(stdio_bus_t *bus, int level,
                                 const char *message, void *user_data);

/**
 * @brief Worker event callback - called on worker lifecycle events
 *
 * @param bus       The stdio_bus instance
 * @param worker_id Worker identifier
 * @param event     Event type: "started", "stopped", "restarting", "failed"
 * @param user_data User context from options
 */
typedef void (*stdio_bus_worker_cb)(stdio_bus_t *bus, int worker_id,
                                    const char *event, void *user_data);

/*============================================================================
 * Listener Configuration (TCP/Unix/Embedded modes)
 *============================================================================*/

/**
 * @brief Listener mode for external client connections
 */
typedef enum {
    STDIO_BUS_LISTEN_NONE = 0,    /**< Embedded mode: no external listener (default) */
    STDIO_BUS_LISTEN_TCP,         /**< TCP socket listener */
    STDIO_BUS_LISTEN_UNIX         /**< Unix domain socket listener */
} stdio_bus_listen_mode_t;

/**
 * @brief Listener configuration
 *
 * Configures how external clients connect to the bus.
 * If mode is NONE, the bus operates in embedded mode where the host
 * application sends/receives messages directly via stdio_bus_ingest().
 */
typedef struct stdio_bus_listener_config {
    stdio_bus_listen_mode_t mode;   /**< Listener mode */
    const char *tcp_host;           /**< TCP bind address (e.g., "0.0.0.0", "127.0.0.1") */
    uint16_t tcp_port;              /**< TCP port number */
    const char *unix_path;          /**< Unix socket path */
} stdio_bus_listener_config_t;

/*============================================================================
 * Client Connection Callbacks (for TCP/Unix modes)
 *============================================================================*/

/**
 * @brief Client connect callback - called when a client connects
 *
 * Only called in TCP/Unix listener modes.
 *
 * @param bus       The stdio_bus instance
 * @param client_id Unique client identifier
 * @param peer_info Peer address info (e.g., "192.168.1.1:54321" or "unix")
 * @param user_data User context from options
 */
typedef void (*stdio_bus_client_connect_cb)(stdio_bus_t *bus, int client_id,
                                            const char *peer_info, void *user_data);

/**
 * @brief Client disconnect callback - called when a client disconnects
 *
 * Only called in TCP/Unix listener modes.
 *
 * @param bus       The stdio_bus instance
 * @param client_id Client identifier
 * @param reason    Disconnect reason (e.g., "closed", "error", "timeout")
 * @param user_data User context from options
 */
typedef void (*stdio_bus_client_disconnect_cb)(stdio_bus_t *bus, int client_id,
                                               const char *reason, void *user_data);

/*============================================================================
 * Configuration Options
 *============================================================================*/

/**
 * @brief Options for creating a stdio_bus instance
 */
typedef struct stdio_bus_options {
    /* Configuration source (one of these must be set) */
    const char *config_path;        /**< Path to JSON config file */
    const char *config_json;        /**< Inline JSON config string (alternative to path) */
    
    /* Listener configuration (optional, default: embedded mode) */
    stdio_bus_listener_config_t listener;  /**< External client listener config */
    
    /* Callbacks (on_message is required) */
    stdio_bus_message_cb on_message;  /**< Called when message ready for host (required) */
    stdio_bus_error_cb on_error;      /**< Called on errors (optional) */
    stdio_bus_log_cb on_log;          /**< Called for log messages (optional, default: stderr) */
    stdio_bus_worker_cb on_worker;    /**< Called on worker events (optional) */
    
    /* Client lifecycle callbacks (optional, for TCP/Unix modes) */
    stdio_bus_client_connect_cb on_client_connect;      /**< Called on client connect */
    stdio_bus_client_disconnect_cb on_client_disconnect; /**< Called on client disconnect */
    
    /* User context passed to all callbacks */
    void *user_data;
    
    /* Optional overrides */
    int log_level;                  /**< 0=DEBUG, 1=INFO, 2=WARN, 3=ERROR (default: 1) */
} stdio_bus_options_t;

/*============================================================================
 * Error Codes (use values from stdio_bus.h, not redefine)
 *============================================================================*/

#define STDIO_BUS_ERR_CONFIG     -10   /**< Configuration error */
#define STDIO_BUS_ERR_WORKER     -11   /**< Worker spawn/communication error */
#define STDIO_BUS_ERR_ROUTING    -12   /**< Message routing error */
#define STDIO_BUS_ERR_BUFFER     -13   /**< Buffer overflow */
#define STDIO_BUS_ERR_INVALID    -14   /**< Invalid argument */
#define STDIO_BUS_ERR_STATE      -15   /**< Invalid state for operation */

/*============================================================================
 * Bus State
 *============================================================================*/

typedef enum {
    STDIO_BUS_STATE_CREATED,      /**< Created but not started */
    STDIO_BUS_STATE_STARTING,     /**< Workers being spawned */
    STDIO_BUS_STATE_RUNNING,      /**< Running and accepting messages */
    STDIO_BUS_STATE_STOPPING,     /**< Graceful shutdown in progress */
    STDIO_BUS_STATE_STOPPED       /**< Fully stopped */
} stdio_bus_state_t;

/*============================================================================
 * Lifecycle Functions
 *============================================================================*/

/**
 * @brief Create a new stdio_bus instance
 *
 * Parses configuration and allocates all internal structures.
 * Workers are NOT started until stdio_bus_start() is called.
 *
 * @param options Configuration options (on_message callback required)
 * @return New instance, or NULL on error (check errno or on_error callback)
 */
stdio_bus_t *stdio_bus_create(const stdio_bus_options_t *options);

/**
 * @brief Start the stdio_bus instance
 *
 * Spawns all worker processes defined in configuration.
 * After this call, the bus is ready to accept messages.
 *
 * @param bus Instance to start
 * @return STDIO_BUS_OK on success, error code on failure
 */
int stdio_bus_start(stdio_bus_t *bus);

/**
 * @brief Process pending I/O (non-blocking)
 *
 * This is the main "pump" function. Call it regularly from your event loop.
 * It processes:
 * - Incoming data from workers
 * - Outgoing data to workers
 * - Worker lifecycle events (SIGCHLD)
 * - Backpressure timeouts
 *
 * @param bus Instance to step
 * @param timeout_ms Maximum time to wait for events:
 *                   - 0: non-blocking, return immediately
 *                   - >0: wait up to this many milliseconds
 *                   - -1: block until event (not recommended for embedding)
 * @return Number of events processed, or negative error code
 */
int stdio_bus_step(stdio_bus_t *bus, int timeout_ms);

/**
 * @brief Initiate graceful shutdown
 *
 * Sends SIGTERM to all workers and starts drain timeout.
 * Continue calling stdio_bus_step() until state becomes STOPPED.
 *
 * @param bus Instance to stop
 * @param timeout_sec Maximum time to wait for workers to exit
 * @return STDIO_BUS_OK on success
 */
int stdio_bus_stop(stdio_bus_t *bus, int timeout_sec);

/**
 * @brief Destroy instance and free all resources
 *
 * If still running, performs immediate (non-graceful) shutdown.
 * After this call, the bus pointer is invalid.
 *
 * @param bus Instance to destroy (may be NULL)
 */
void stdio_bus_destroy(stdio_bus_t *bus);

/*============================================================================
 * Message Functions
 *============================================================================*/

/**
 * @brief Send a message into the bus (from host to workers)
 *
 * The message is routed to the appropriate worker based on sessionId.
 * Responses will be delivered via the on_message callback.
 *
 * @param bus Instance
 * @param msg JSON message (must be valid JSON-RPC)
 * @param len Message length
 * @return STDIO_BUS_OK on success, error code on failure
 */
int stdio_bus_ingest(stdio_bus_t *bus, const char *msg, size_t len);

/*============================================================================
 * Query Functions
 *============================================================================*/

/**
 * @brief Get current bus state
 *
 * @param bus Instance
 * @return Current state
 */
stdio_bus_state_t stdio_bus_get_state(const stdio_bus_t *bus);

/**
 * @brief Get number of active workers
 *
 * @param bus Instance
 * @return Number of workers in RUNNING state
 */
int stdio_bus_worker_count(const stdio_bus_t *bus);

/**
 * @brief Get number of active sessions
 *
 * @param bus Instance
 * @return Number of active sessions
 */
int stdio_bus_session_count(const stdio_bus_t *bus);

/**
 * @brief Get number of pending requests
 *
 * @param bus Instance
 * @return Number of requests awaiting response
 */
int stdio_bus_pending_count(const stdio_bus_t *bus);

/**
 * @brief Get number of connected clients (TCP/Unix modes only)
 *
 * @param bus Instance
 * @return Number of connected clients, or 0 in embedded mode
 */
int stdio_bus_client_count(const stdio_bus_t *bus);

/**
 * @brief Get the underlying event loop file descriptor
 *
 * This is useful for integrating with external event loops (e.g., libuv).
 * The returned fd can be polled for readability to know when stdio_bus_step()
 * should be called.
 *
 * @param bus Instance
 * @return File descriptor (epoll_fd or kqueue_fd), or -1 if not available
 */
int stdio_bus_get_poll_fd(const stdio_bus_t *bus);

/*============================================================================
 * Embedded Worker Functions (for N-API / language runtime integration)
 *============================================================================*/

/**
 * @brief Register an embedded worker with pre-existing socketpair fds
 *
 * Creates a worker slot backed by socketpair fds instead of a spawned process.
 * The worker gets pid=-1 (sentinel), state=RUNNING, embedded=true.
 * Existing pid guards (pid > 0) protect against SIGTERM/SIGKILL/waitpid.
 *
 * The caller (JS/Python runtime) owns the "other side" of the socketpair.
 * The kernel owns the fds passed here.
 *
 * @param bus Instance (must be in RUNNING state)
 * @param fd_to_worker FD for kernel to write to embedded worker
 * @param fd_from_worker FD for kernel to read from embedded worker
 * @param pool_id Pool identifier for this embedded worker
 * @return Worker ID (>= 0) on success, negative error code on failure
 */
int stdio_bus_register_embedded_worker(stdio_bus_t *bus,
                                       int fd_to_worker,
                                       int fd_from_worker,
                                       const char *pool_id);

/**
 * @brief Unregister an embedded worker
 *
 * Removes fds from event loop BEFORE closing (prevents use-after-close).
 * Closes kernel-side fds only. Sets state=STOPPED. Idempotent.
 *
 * @param bus Instance
 * @param worker_id Worker ID returned by register
 * @return STDIO_BUS_OK on success, negative error code on failure
 */
int stdio_bus_unregister_embedded_worker(stdio_bus_t *bus, int worker_id);

/*============================================================================
 * Statistics (optional)
 *============================================================================*/

/**
 * @brief Statistics structure
 */
typedef struct stdio_bus_stats {
    uint64_t messages_in;           /**< Messages received from host/clients */
    uint64_t messages_out;          /**< Messages sent to host/clients */
    uint64_t bytes_in;              /**< Total bytes received */
    uint64_t bytes_out;             /**< Total bytes sent */
    uint64_t worker_restarts;       /**< Total worker restarts */
    uint64_t routing_errors;        /**< Messages that couldn't be routed */
    uint64_t client_connects;       /**< Total client connections (TCP/Unix) */
    uint64_t client_disconnects;    /**< Total client disconnections (TCP/Unix) */
} stdio_bus_stats_t;

/**
 * @brief Get statistics
 *
 * @param bus Instance
 * @param stats Output structure
 */
void stdio_bus_get_stats(const stdio_bus_t *bus, stdio_bus_stats_t *stats);

#ifdef __cplusplus
}
#endif

#endif /* STDIO_BUS_EMBED_H */
