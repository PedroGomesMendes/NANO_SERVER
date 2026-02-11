idf_component_register( SRCS "hello_world_main.c"
                        PRIV_REQUIRES spi_flash
                        REQUIRES esp_wifi
                        REQUIRES nvs_flash
                        REQUIRES esp_http_server
                        REQUIRES driver
                        REQUIRES esp_psram 
                        INCLUDE_DIRS ""
                        EMBED_FILES "index.html"
                       )
