#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#ifdef __cplusplus
extern "C" {
#endif

// Start the HTTP server
void http_server_start(void);

// Stop the HTTP server
void http_server_stop(void);

#ifdef __cplusplus
}
#endif

#endif // HTTP_SERVER_H 