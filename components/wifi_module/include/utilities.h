#ifndef T_UTILITIES_H
#define T_UTILITIES_H

//A generic callback for stop actions
typedef void (*stop_action_cb_t) (void);

/**
 * @brief Starts a global provisioning timer.
 * @param timeout_min Minutes until the timer expires.
 */
void start_provisioning_manager(int timeout_min);

/**
 * @brief Register a module (WiFi, BLE, WebServer) to be stopped on timeout.
 * @param cb The function to call when time is up.
 */
void register_stop_component(stop_action_cb_t cb);

/**
 * @brief Cancels the timer (call this when credentials are successfully received).
 */
void stop_provisioning_manager(void);

#endif