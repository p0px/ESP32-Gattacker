#include "wifi/routes.h"

#define TAG "ROUTE"

#define REST_CHECK(a, str, goto_tag, ...)                                            \
  do                                                                                 \
  {                                                                                  \
    if (!(a))                                                                        \
    {                                                                                \
      ESP_LOGE(TAG, "%s(%d): " str, __FUNCTION__, __LINE__, ##__VA_ARGS__);          \
      goto goto_tag;                                                                 \
    }                                                                                \
  } while (0)

#define FILE_PATH_MAX (ESP_VFS_PATH_MAX + 256)
#define SCRATCH_BUFSIZE (10240)

typedef struct rest_server_context {
  char base_path[ESP_VFS_PATH_MAX + 1];
  char scratch[SCRATCH_BUFSIZE];
} rest_server_context_t;

#define CHECK_FILE_EXTENSION(filename, ext) (strcasecmp(&filename[strlen(filename) - strlen(ext)], ext) == 0)

/* Set HTTP response content type according to file extension */
static esp_err_t set_content_type_from_file(httpd_req_t *req, const char *filepath) {
  const char *type = "text/plain";

  if (CHECK_FILE_EXTENSION(filepath, ".html")) {
    type = "text/html";
  } else if (CHECK_FILE_EXTENSION(filepath, ".js")) {
    type = "application/javascript";
  } else if (CHECK_FILE_EXTENSION(filepath, ".css")) {
    type = "text/css";
  } else if (CHECK_FILE_EXTENSION(filepath, ".png")) {
    type = "image/png";
  } else if (CHECK_FILE_EXTENSION(filepath, ".ico")) {
    type = "image/x-icon";
  } else if (CHECK_FILE_EXTENSION(filepath, ".svg")) {
    type = "text/xml";
  }
  return httpd_resp_set_type(req, type);
}

/* main httpd route handler */
static esp_err_t main_handler(httpd_req_t *req) {
  char filepath[FILE_PATH_MAX];
  rest_server_context_t *rest_context = (rest_server_context_t *)req->user_ctx;

  // Construct the base path
  strlcpy(filepath, rest_context->base_path, sizeof(filepath));

  // Handle path ending with "/" by appending "index.html"
  if (req->uri[strlen(req->uri) - 1] == '/') {
    strlcat(filepath, "/index.html", sizeof(filepath));
  } else {
    // Regular path handling
    strlcat(filepath, req->uri, sizeof(filepath));
  }

  // Special handling for 204, some captive portal checkes will send this
  if (strstr(req->uri, "204")) {
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "http://1.3.3.7");
    return httpd_resp_send(req, NULL, 0);
  }

  // Create a gz_filepath
  char gz_filepath[FILE_PATH_MAX];
  strlcpy(gz_filepath, filepath, sizeof(gz_filepath));

  if (strlcat(gz_filepath, ".gz", sizeof(gz_filepath)) >= sizeof(gz_filepath)) {
    // Handle case where gz_filepath is too long to append ".gz"
    ESP_LOGE(TAG, "File path too long to append '.gz': %s", gz_filepath);
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "File path too long");
    return ESP_FAIL;
  }

  // Check if gzipped file exists
  struct stat file_stat;
  const char *final_filepath = filepath;  // Default to non-gzipped file

  if (stat(gz_filepath, &file_stat) == 0) {
    // Gzipped file exists
    final_filepath = gz_filepath;
    httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
  } else if (stat(filepath, &file_stat) != 0) {
    // Neither gzipped nor regular file found
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "http://1.3.3.7");
    return httpd_resp_send(req, "Redirect", HTTPD_RESP_USE_STRLEN);
  }

  // Open the file
  int fd = open(final_filepath, O_RDONLY);
  if (fd == -1) {
    ESP_LOGE(TAG, "Failed to open file: %s", final_filepath);
    httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "File not found");
    return ESP_FAIL;
  }

  // Set the content type based on the file extension
  set_content_type_from_file(req, filepath);

  // Send the file in chunks
  char *chunk = rest_context->scratch;
  ssize_t read_bytes;
  do {
    read_bytes = read(fd, chunk, SCRATCH_BUFSIZE);
    if (read_bytes > 0) {
      if (httpd_resp_send_chunk(req, chunk, read_bytes) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send file: %s", final_filepath);
        close(fd);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to send file");
        return ESP_FAIL;
      }
    }
  } while (read_bytes > 0);

  // Close the file and finalize the response
  close(fd);
  httpd_resp_send_chunk(req, NULL, 0);

  return ESP_OK;
}

/* initialize littlefs filesystem */
static esp_err_t init_fs(void) {
  esp_vfs_littlefs_conf_t conf = {
    .base_path = BASE_PATH,
    .partition_label = "spiffs",
    .partition = NULL,
    .format_if_mount_failed = false,
    .read_only = true,
    .dont_mount = false,
    .grow_on_mount = true
  };
  esp_err_t ret = esp_vfs_littlefs_register(&conf);

  if (ret != ESP_OK) {
    if (ret == ESP_FAIL) {
      ESP_LOGE(TAG, "Failed to mount or format filesystem");
    } else if (ret == ESP_ERR_NOT_FOUND) {
      ESP_LOGE(TAG, "Failed to find SPIFFS partition");
    } else {
      ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
    }
    return ESP_FAIL;
  }

  size_t total = 0, used = 0;
  ret = esp_littlefs_info("spiffs", &total, &used);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
  } else {
    ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
  }
  return ESP_OK;
}

/**
 * start_rest_server
 * 
 * Starts the HTTPD server.
 **/
esp_err_t start_rest_server() {
  init_fs();
  
  rest_server_context_t *rest_context = NULL;
  httpd_handle_t server = NULL;
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.close_fn = ws_on_close_handler;
  config.uri_match_fn = httpd_uri_match_wildcard;
  rest_context = (rest_server_context_t *)calloc(1, sizeof(rest_server_context_t));

  // main web server handler
  static const httpd_uri_t app = {
    .uri = "/*",
    .method = HTTP_GET,
    .handler = main_handler,
    .user_ctx = rest_context,
    .is_websocket = false,
  };

  // websocket handler
  static const httpd_uri_t ws = {
    .uri = "/ws",
    .method = HTTP_GET,
    .handler = ws_handler,
    .user_ctx = rest_context,
    .is_websocket = true
  };

  // Allocate memory for rest_context    
  REST_CHECK(rest_context, "No memory for rest context", err);

  // Copy the base path to rest_context
  strlcpy(rest_context->base_path, BASE_PATH, sizeof(rest_context->base_path));

  // Start the HTTP server
  ESP_LOGI(TAG, "Starting HTTP Server");
  REST_CHECK(httpd_start(&server, &config) == ESP_OK, "Start server failed", err_start);

  httpd_register_uri_handler(server, &ws);
  httpd_register_uri_handler(server, &app);

  return ESP_OK;

err_start:
  free(rest_context);
err:
  return ESP_FAIL;
}
