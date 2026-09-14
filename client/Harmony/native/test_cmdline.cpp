#include <freerdp/client.h>
#include <freerdp/client/cmdline.h>
#include <freerdp/settings.h>
#include <stdio.h>
#include <vector>
#include <string>
#include <iostream>

int main() {
    rdpSettings* settings = freerdp_settings_new(0);
    std::vector<std::string> args;
    args.emplace_back("freerdp-harmony");
    args.emplace_back("/v:127.0.0.1:3389");
    args.emplace_back("-clipboard");
    args.emplace_back("/audio-mode:2");
    args.emplace_back("/cert:ignore");
    args.emplace_back("/gfx");
    args.emplace_back("/dynamic-resolution");

    std::vector<char*> argv;
    for (auto& arg : args) {
        argv.push_back(&arg[0]);
    }

    int rc = freerdp_client_settings_parse_command_line(settings, argv.size(), argv.data(), FALSE);
    if (rc < 0) {
        printf("freerdp_client_settings_parse_command_line failed: %d\n", rc);
    } else {
        printf("freerdp_client_settings_parse_command_line success\n");
    }
    freerdp_settings_free(settings);
    return 0;
}