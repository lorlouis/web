#include <stddef.h>

const char I_AM_A_TEAPOT[] = (
    "<!DOCTYPE html>"
        "<html>"
            "<head>"
                "<title>teapot</title>"
            "</head>"
            "<body>"
                "<h1 style=\"text-align: center;\">I'm a teapot</h1>"
                "<h1 style=\"text-align: center;\">"
                    "<code><strong>{(&macr;)/&acute;</strong></code>"
                "</h1>"
                "<h3 style=\"text-align: center;\">"
                    "Will you have a cup of Tea?"
                "</h3>"
            "</body>"
        "</html>"
);
const size_t I_AM_A_TEAPOT_LEN = sizeof(I_AM_A_TEAPOT);

const char NOT_FOUND_PAGE[] = (
    "<!DOCTYPE html>"
        "<html>"
            "<head>"
                "<title>404</title>"
            "</head>"
            "<body>"
                "<h1>NOT_FOUND</h1>"
                "<p>The resource asked for could not be found</p>"
            "</body>"
        "</html>"
);
const size_t NOT_FOUND_PAGE_LEN = sizeof(NOT_FOUND_PAGE);

const char UNIMPLEMENTED_PAGE[] = (
    "<!DOCTYPE html>"
        "<html>"
            "<head>"
                "<title>405</title>"
            "</head>"
            "<body>"
                "<h1>UNIMPLEMENTED</h1>"
                "<p>this server only implements http GET</p>"
            "</body>"
        "</html>"
);
const size_t UNIMPLEMENTED_PAGE_LEN = sizeof(UNIMPLEMENTED_PAGE);

const char SERVER_ERROR_PAGE[] = (
    "<!DOCTYPE html>"
        "<html>"
            "<head>"
                "<title>500</title>"
            "</head>"
            "<body>"
                "<h1>SERVER ERROR</h1>"
                "<p>server error check the server's logs for more info</p>"
            "</body>"
        "</html>"
);
const size_t SERVER_ERROR_PAGE_LEN = sizeof(SERVER_ERROR_PAGE);

const char UPGRADE_REQUIRED_PAGE[] = (
    "<!DOCTYPE html>"
        "<html>"
            "<head>"
                "<title>426</title>"
            "</head>"
            "<body>"
                "<h1>UPGRADE REQUIRED</h1>"
                "<p>The resource requested is not available under the current protocol.</p>"
            "</body>"
        "</html>"
);
const size_t UPGRADE_REQUIRED_PAGE_LEN = sizeof(UPGRADE_REQUIRED_PAGE);
