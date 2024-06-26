#include <nng/nng.h>
#include <nng/supplemental/http/http.h>
#include <stdio.h>
#include <stdlib.h>

#include "logger.h"

void
fatal(int rv)
{
	fprintf(stderr, "%s\n", nng_strerror(rv));
	// exit(1);
}

int
http_get(const char* url_req )
{
	nng_http_client *client;
	nng_http_conn *  conn;
	nng_url *        url;
	nng_aio *        aio;
	nng_http_req *   req;
	nng_http_res *   res;
	const char *     hdr;
	int              rv;
	int              len;
	void *           data;
	nng_iov          iov;
	
	if (((rv = nng_url_parse(&url, url_req)) != 0) ||
	    ((rv = nng_http_client_alloc(&client, url)) != 0) ||
	    ((rv = nng_http_req_alloc(&req, url)) != 0) ||
	    ((rv = nng_http_res_alloc(&res)) != 0) ||
	    ((rv = nng_aio_alloc(&aio, NULL, NULL)) != 0)) 
	{
		fatal(rv);
	}

	// Start connection process...
	nng_http_client_connect(client, aio);

	// Wait for it to finish.
	nng_aio_wait(aio);
	if ((rv = nng_aio_result(aio)) != 0) 
	{
		fatal(rv);
	}

	// Get the connection, at the 0th output.
	conn = nng_aio_get_output(aio, 0);

	// Request is already set up with URL, and for GET via HTTP/1.1.
	// The Host: header is already set up too.

	// Send the request, and wait for that to finish.
	nng_http_conn_write_req(conn, req, aio);
	nng_aio_wait(aio);

	if ((rv = nng_aio_result(aio)) != 0) 
	{
		fatal(rv);
	}

	// Read a response.
	nng_http_conn_read_res(conn, res, aio);
	nng_aio_wait(aio);

	if ((rv = nng_aio_result(aio)) != 0) 
	{
		fatal(rv);
	}

	if (nng_http_res_get_status(res) != NNG_HTTP_STATUS_OK) 
	{
		fprintf(stderr, "HTTP Server Responded: %d %s\n",
		    nng_http_res_get_status(res),
		    nng_http_res_get_reason(res));
	}

	// This only supports regular transfer encoding (no Chunked-Encoding,
	// and a Content-Length header is required.)
	if ((hdr = nng_http_res_get_header(res, "Content-Length")) == NULL) 
	{
		fprintf(stderr, "Missing Content-Length header.\n");
		exit(1);
	}

	len = atoi(hdr);
	if (len == 0) {
		return (0);
	}

	// Allocate a buffer to receive the body data.
	data = malloc(len);

	// Set up a single iov to point to the buffer.
	iov.iov_len = len;
	iov.iov_buf = data;

	// Following never fails with fewer than 5 elements.
	nng_aio_set_iov(aio, 1, &iov);

	// Now attempt to receive the data.
	nng_http_conn_read_all(conn, aio);

	// Wait for it to complete.
	nng_aio_wait(aio);

	if ((rv = nng_aio_result(aio)) != 0) 
	{
		fatal(rv);
	}

	// write to stdout
	fwrite(data, 1, len, stdout);

	return (0);
}

static void
send_msg(const char* url_addr, nng_msg *msg)
{
	nng_http_client *client = NULL;
	nng_http_conn *  conn   = NULL;
	nng_url *        url    = NULL;
	nng_aio *        aio    = NULL;
	nng_http_req *   req    = NULL;
	nng_http_res *   res    = NULL;
	int              rv;

	if (((rv = nng_url_parse(&url, url_addr)) != 0) ||
	    ((rv = nng_http_client_alloc(&client, url)) != 0) ||
	    ((rv = nng_http_req_alloc(&req, url)) != 0) ||
	    ((rv = nng_http_res_alloc(&res)) != 0) ||
	    ((rv = nng_aio_alloc(&aio, NULL, NULL)) != 0)) 
	{
		log_message(LOG_ERR, "init failed: %s\n", nng_strerror(rv));
		goto out;
	}

	// Start connection process...
	nng_aio_set_timeout(aio, 1000);
	nng_http_client_connect(client, aio);

	// Wait for it to finish.

	nng_aio_wait(aio);
	if ((rv = nng_aio_result(aio)) != 0) 
	{
		log_message(LOG_ERR, "Connect failed: %s", nng_strerror(rv));
		nng_aio_finish(aio, rv);

		goto out;
	}

	// Get the connection, at the 0th output.
	conn = nng_aio_get_output(aio, 0);

	// Request is already set up with URL, and for GET via HTTP/1.1.
	// The Host: header is already set up too.
	// set_data(req, conf_req, params);
	// Send the request, and wait for that to finish.
	// for (size_t i = 0; i < conf->header_count; i++) 
	// {
	// 	nng_http_req_add_header(
	// 	    req, conf->headers[i]->key, conf->headers[i]->value);
	// }

	nng_http_req_set_method(req, "POST");
	nng_http_req_set_data(req, nng_msg_body(msg), nng_msg_len(msg));
	nng_http_conn_write_req(conn, req, aio);
	nng_aio_set_timeout(aio, 1000);
	nng_aio_wait(aio);

	if ((rv = nng_aio_result(aio)) != 0) {
		log_message(LOG_ERR, "Write req failed: %s", nng_strerror(rv));
		nng_aio_finish(aio, rv);

		goto out;
	}

out:
	if (url) {
		nng_url_free(url);
	}
	if (conn) {
		nng_http_conn_close(conn);
	}
	if (client) {
		nng_http_client_free(client);
	}
	if (req) {
		nng_http_req_free(req);
	}
	if (res) {
		nng_http_res_free(res);
	}
	if (aio) {
		nng_aio_free(aio);
	}
}
